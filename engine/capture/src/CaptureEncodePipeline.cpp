#include "mist/capture/CaptureEncodePipeline.h"
#include "mist/capture/CaptureEngineFactory.h"
#include "mist/capture/WgcCaptureEngine.h"
#include "mist/common/Logger.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace mist::pipeline {

std::string PipelineReport::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";
    ss << "  \"hardware\": {\n";
    ss << "    \"gpu\": \"" << hardwareGpu << "\",\n";
    ss << "    \"driver\": \"" << driverVersion << "\"\n";
    ss << "  },\n";
    ss << "  \"stream\": {\n";
    ss << "    \"width\": " << width << ",\n";
    ss << "    \"height\": " << height << ",\n";
    ss << "    \"target_fps\": " << targetFps << ",\n";
    ss << "    \"actual_fps\": " << actualFps << ",\n";
    ss << "    \"avg_bitrate_kbps\": " << averageBitrateKbps << "\n";
    ss << "  },\n";
    ss << "  \"counters\": {\n";
    ss << "    \"frames_acquired\": " << framesAcquired << ",\n";
    ss << "    \"frames_encoded\": " << framesEncoded << ",\n";
    ss << "    \"frames_dropped\": " << framesDropped << ",\n";
    ss << "    \"capture_timeouts\": " << captureTimeouts << ",\n";
    ss << "    \"capture_errors\": " << captureErrors << ",\n";
    ss << "    \"encode_errors\": " << encodeErrors << "\n";
    ss << "  },\n";
    ss << "  \"latencies_us\": {\n";
    ss << "    \"capture\": {\n";
    ss << "      \"min\": " << captureLatencyUs.minUs << ", \"max\": " << captureLatencyUs.maxUs
       << ", \"avg\": " << captureLatencyUs.avgUs << ", \"p50\": " << captureLatencyUs.p50Us
       << ", \"p95\": " << captureLatencyUs.p95Us << ", \"p99\": " << captureLatencyUs.p99Us << "\n";
    ss << "    },\n";
    ss << "    \"conversion\": {\n";
    ss << "      \"min\": " << conversionLatencyUs.minUs << ", \"max\": " << conversionLatencyUs.maxUs
       << ", \"avg\": " << conversionLatencyUs.avgUs << ", \"p50\": " << conversionLatencyUs.p50Us
       << ", \"p95\": " << conversionLatencyUs.p95Us << ", \"p99\": " << conversionLatencyUs.p99Us << "\n";
    ss << "    },\n";
    ss << "    \"encode\": {\n";
    ss << "      \"min\": " << encodeLatencyUs.minUs << ", \"max\": " << encodeLatencyUs.maxUs
       << ", \"avg\": " << encodeLatencyUs.avgUs << ", \"p50\": " << encodeLatencyUs.p50Us
       << ", \"p95\": " << encodeLatencyUs.p95Us << ", \"p99\": " << encodeLatencyUs.p99Us << "\n";
    ss << "    },\n";
    ss << "    \"capture_to_bitstream\": {\n";
    ss << "      \"min\": " << captureToBitstreamLatencyUs.minUs << ", \"max\": " << captureToBitstreamLatencyUs.maxUs
       << ", \"avg\": " << captureToBitstreamLatencyUs.avgUs << ", \"p50\": " << captureToBitstreamLatencyUs.p50Us
       << ", \"p95\": " << captureToBitstreamLatencyUs.p95Us << ", \"p99\": " << captureToBitstreamLatencyUs.p99Us << "\n";
    ss << "    }\n";
    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

CaptureEncodePipeline::CaptureEncodePipeline() noexcept {
    auto& reg = telemetry::MetricRegistry::instance();
    histCaptureLatency_ = reg.getOrCreateHistogram("pipeline_capture_latency_us");
    histConversionLatency_ = reg.getOrCreateHistogram("pipeline_conversion_latency_us");
    histEncodeLatency_ = reg.getOrCreateHistogram("pipeline_encode_latency_us");
    histCaptureToBitstream_ = reg.getOrCreateHistogram("pipeline_capture_to_bitstream_us");
}

CaptureEncodePipeline::~CaptureEncodePipeline() {
    (void)stop();
}

common::Result<void> CaptureEncodePipeline::initialize(
    const capture::CaptureConfig& capConfig,
    const encode::EncodeConfig& encConfig,
    bool useSyntheticSource
) {
    capConfig_ = capConfig;
    encConfig_ = encConfig;
    useSynthetic_ = useSyntheticSource;

    captureEngine_.reset();
    syntheticGen_.reset();
    colorConverter_.reset();
    encoder_.reset();

    // Reset metrics
    framesAcquired_ = 0;
    framesEncoded_ = 0;
    framesDropped_ = 0;
    captureTimeouts_ = 0;
    captureErrors_ = 0;
    encodeErrors_ = 0;
    totalEncodedBytes_ = 0;

    histCaptureLatency_->reset();
    histConversionLatency_->reset();
    histEncodeLatency_->reset();
    histCaptureToBitstream_->reset();

#if defined(_WIN32)
    ID3D11Device* sharedDevice = nullptr;
    ID3D11DeviceContext* sharedContext = nullptr;

    if (!useSynthetic_) {
        auto [engine, capRes] = capture::CaptureEngineFactory::createAndInitialize(
            capConfig_, capture::CaptureBackend::DxgiDuplication, true
        );
        if (!capRes.hasValue() || !engine) {
            return capRes;
        }
        captureEngine_ = std::move(engine);
        if (captureEngine_->getBackend() == capture::CaptureBackend::DxgiDuplication) {
            auto* dxgi = static_cast<capture::DxgiCaptureEngine*>(captureEngine_.get());
            sharedDevice = dxgi->getD3D11Device();
            sharedContext = dxgi->getD3D11Context();
        } else if (captureEngine_->getBackend() == capture::CaptureBackend::WindowsGraphicsCapture) {
            auto* wgc = static_cast<capture::WgcCaptureEngine*>(captureEngine_.get());
            sharedDevice = wgc->getD3D11Device();
            sharedContext = wgc->getD3D11Context();
        }
    } else {
        auto devRes = platform::windows::D3D11DeviceManager::createDevice(0);
        if (!devRes) {
            return devRes.error();
        }
        syntheticGen_ = std::make_unique<capture::SyntheticFrameGenerator>();
        auto synRes = syntheticGen_->initialize(devRes.value().first.Get(), devRes.value().second.Get(), encConfig_.width, encConfig_.height);
        if (!synRes) {
            return synRes;
        }
        sharedDevice = devRes.value().first.Get();
        sharedContext = devRes.value().second.Get();
    }

    // Initialize ColorConverter for BGRA to NV12 if needed
    colorConverter_ = std::make_unique<capture::ColorConverter>();
    (void)colorConverter_->initialize(sharedDevice, sharedContext, encConfig_.width, encConfig_.height);

    // Initialize video encoder (NVENC, AMF, QuickSync, or Software fallback)
    auto encRes = encode::VideoEncoderFactory::createEncoder(encode::EncoderBackend::Auto, sharedDevice);
    if (!encRes) {
        return encRes.error();
    }
    encoder_ = std::move(encRes.value());
    auto initRes = encoder_->initializeWithDevice(encConfig_, sharedDevice, sharedContext);
    if (!initRes) {
        return initRes;
    }

    MIST_LOG_INFO(common::SubsystemId::Common, "CaptureEncodePipeline initialized successfully.");
    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Common, common::ErrorCode::NotImplemented, "Pipeline only supported on Windows");
#endif
}

common::Result<void> CaptureEncodePipeline::start() {
    if (isRunning_.load()) {
        return common::Result<void>::success();
    }

    (void)stop();

    stopRequested_.store(false);
    isRunning_.store(true);
    pipelineTimer_.reset();

    if (captureEngine_) {
        auto res = captureEngine_->start();
        if (!res) {
            isRunning_.store(false);
            return res;
        }
    }

    captureThread_ = std::thread(&CaptureEncodePipeline::captureWorker, this);
    encodeThread_ = std::thread(&CaptureEncodePipeline::encodeWorker, this, "");

    MIST_LOG_INFO(common::SubsystemId::Common, "CaptureEncodePipeline threads started.");
    return common::Result<void>::success();
}

common::Result<void> CaptureEncodePipeline::stop() {
    stopRequested_.store(true);
    queueCv_.notify_all();

    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    if (encodeThread_.joinable()) {
        encodeThread_.join();
    }

    if (!isRunning_.exchange(false)) {
        return common::Result<void>::success();
    }

    if (captureEngine_) {
        (void)captureEngine_->stop();
    }
    if (encoder_) {
        (void)encoder_->flush();
    }

    MIST_LOG_INFO(common::SubsystemId::Common, "CaptureEncodePipeline stopped.");
    return common::Result<void>::success();
}

common::Result<PipelineReport> CaptureEncodePipeline::runForDuration(
    uint32_t durationSeconds,
    const std::string& outputH264Path
) {
    (void)stop();

    stopRequested_.store(false);
    isRunning_.store(true);
    pipelineTimer_.reset();

    if (captureEngine_) {
        auto res = captureEngine_->start();
        if (!res) {
            isRunning_.store(false);
            return res.error();
        }
    }

    captureThread_ = std::thread(&CaptureEncodePipeline::captureWorker, this);
    encodeThread_ = std::thread(&CaptureEncodePipeline::encodeWorker, this, outputH264Path);

    MIST_LOG_INFO(common::SubsystemId::Common, "Running pipeline benchmark for " + std::to_string(durationSeconds) + " seconds...");

    // Wait for requested duration or until error
    const auto startTime = std::chrono::steady_clock::now();
    while (!stopRequested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count();
        if (elapsedSec >= durationSeconds) {
            break;
        }
    }

    (void)stop();

    PipelineReport report = generateReport();
    return report;
}

void CaptureEncodePipeline::captureWorker() {
    platform::windows::HighResolutionTimer qpc;
    platform::windows::FramePacer pacer(capConfig_.targetFps > 0 ? capConfig_.targetFps : 60);
    uint64_t frameSeq = 0;

    const uint32_t frameIntervalMs = 1000 / capConfig_.targetFps;

    while (!stopRequested_.load()) {
        qpc.reset();
        common::Result<capture::CapturedFrame> frameRes = captureEngine_
            ? captureEngine_->acquireNextFrame(frameIntervalMs + 5)
            : (syntheticGen_ ? syntheticGen_->generateFrame(++frameSeq, 0)
                             : common::Error(common::SubsystemId::Capture, common::ErrorCode::NotInitialized, "No capture source"));

        if (syntheticGen_) {
            pacer.waitNextFrame();
        }

        if (!frameRes) {
            if (frameRes.error().code() == common::ErrorCode::Timeout) {
                captureTimeouts_.fetch_add(1, std::memory_order_relaxed);
            } else {
                captureErrors_.fetch_add(1, std::memory_order_relaxed);
            }
            continue;
        }

        auto capturedFrame = frameRes.value();
        capturedFrame.timestampNs = pipelineTimer_.elapsedNanoseconds();

        double captureLatencyUs = qpc.elapsedMicroseconds();
        histCaptureLatency_->record(captureLatencyUs);
        framesAcquired_.fetch_add(1, std::memory_order_relaxed);

        // Bounded queue: max 2 frames. Drop oldest if queue is full.
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            while (pendingFrames_.size() >= 2) {
                pendingFrames_.pop();
                framesDropped_.fetch_add(1, std::memory_order_relaxed);
            }
            pendingFrames_.push(std::move(capturedFrame));
        }
        queueCv_.notify_one();
    }
}

void CaptureEncodePipeline::encodeWorker(const std::string& outputH264Path) {
    platform::windows::HighResolutionTimer qpc;
    std::ofstream bitstreamFile;
    if (!outputH264Path.empty()) {
        bitstreamFile.open(outputH264Path, std::ios::binary);
    }

    while (!stopRequested_.load() || !pendingFrames_.empty()) {
        capture::CapturedFrame frame;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait_for(lock, std::chrono::milliseconds(50), [this]() {
                return !pendingFrames_.empty() || stopRequested_.load();
            });

            if (pendingFrames_.empty()) {
                if (stopRequested_.load()) break;
                continue;
            }

            frame = pendingFrames_.front();
            pendingFrames_.pop();
        }

        // Color conversion if NV12 format is selected
        if (encConfig_.codec == encode::VideoCodec::H264 && colorConverter_) {
            qpc.reset();
#if defined(_WIN32)
            auto convRes = colorConverter_->convertBgraToNv12(static_cast<ID3D11Texture2D*>(frame.nativeResource));
            if (convRes) {
                frame.nativeResource = convRes.value();
                frame.format = capture::PixelFormat::NV12;
            }
#endif
            histConversionLatency_->record(qpc.elapsedMicroseconds());
        }

        // Encode picture on hardware
        qpc.reset();
        auto encRes = encoder_->encode(frame);
        if (!encRes) {
            encodeErrors_.fetch_add(1, std::memory_order_relaxed);
            MIST_LOG_ERROR(common::SubsystemId::Encode, "encodeWorker frame error: " + encRes.error().format());
            continue;
        }

        double encodeLatencyUs = qpc.elapsedMicroseconds();
        histEncodeLatency_->record(encodeLatencyUs);

        // Retrieve bitstream packet
        auto pktRes = encoder_->getNextPacket();
        if (pktRes) {
            const auto& packet = pktRes.value();
            framesEncoded_.fetch_add(1, std::memory_order_relaxed);
            totalEncodedBytes_.fetch_add(packet.data.size(), std::memory_order_relaxed);

            // Compute capture-to-bitstream latency
            uint64_t nowNs = pipelineTimer_.elapsedNanoseconds();
            if (nowNs >= frame.timestampNs) {
                double captureToBitstreamUs = static_cast<double>(nowNs - frame.timestampNs) / 1000.0;
                histCaptureToBitstream_->record(captureToBitstreamUs);
            }

            if (bitstreamFile.is_open() && !packet.data.empty()) {
                bitstreamFile.write(reinterpret_cast<const char*>(packet.data.data()), static_cast<std::streamsize>(packet.data.size()));
            }

            if (packetCb_ && !packet.data.empty()) {
                packetCb_(packet);
            }
        }
    }
}

void CaptureEncodePipeline::requestKeyframe() noexcept {
    if (encoder_) {
        (void)encoder_->requestKeyframe();
    }
}

common::Result<void> CaptureEncodePipeline::setBitrate(uint32_t bitrateKbps) noexcept {
    if (encoder_) {
        return encoder_->setBitrate(bitrateKbps);
    }
    return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotInitialized, "Encoder not initialized");
}

PipelineReport CaptureEncodePipeline::generateReport() const {
    double elapsedSec = pipelineTimer_.elapsedMilliseconds() / 1000.0;
    if (elapsedSec <= 0.0) elapsedSec = 1.0;

    uint64_t encodedCount = framesEncoded_.load();
    double actualFps = static_cast<double>(encodedCount) / elapsedSec;
    double avgBitrateKbps = (static_cast<double>(totalEncodedBytes_.load() * 8) / elapsedSec) / 1000.0;

    std::string gpuName = "NVIDIA GeForce";
    auto adapters = platform::windows::D3D11DeviceManager::enumerateAdapters();
    if (!adapters.empty()) {
        gpuName = adapters[0].description;
    }

    return PipelineReport{
        .hardwareGpu = gpuName,
        .driverVersion = "NVENC 12.x",
        .width = encConfig_.width,
        .height = encConfig_.height,
        .targetFps = encConfig_.targetFps,
        .actualFps = actualFps,
        .averageBitrateKbps = avgBitrateKbps,
        .framesAcquired = framesAcquired_.load(),
        .framesEncoded = encodedCount,
        .framesDropped = framesDropped_.load(),
        .captureTimeouts = captureTimeouts_.load(),
        .captureErrors = captureErrors_.load(),
        .encodeErrors = encodeErrors_.load(),
        .captureLatencyUs = histCaptureLatency_->snapshot(),
        .conversionLatencyUs = histConversionLatency_->snapshot(),
        .encodeLatencyUs = histEncodeLatency_->snapshot(),
        .captureToBitstreamLatencyUs = histCaptureToBitstream_->snapshot()
    };
}

capture::CaptureBackend CaptureEncodePipeline::getActiveCaptureBackend() const noexcept {
    if (useSynthetic_) {
        return capture::CaptureBackend::Synthetic;
    }
    if (captureEngine_) {
        return captureEngine_->getBackend();
    }
    return capture::CaptureBackend::Synthetic;
}

} // namespace mist::pipeline
