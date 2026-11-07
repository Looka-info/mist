#include "mist/decode/DecodePresentPipeline.h"
#include "mist/common/Logger.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

namespace mist::decode {

std::string DecodeBenchmarkReport::toJson() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";
    ss << "  \"hardware\": {\n";
    ss << "    \"gpu\": \"" << hardwareGpu << "\",\n";
    ss << "    \"decoder_backend\": \"" << decoderBackend << "\"\n";
    ss << "  },\n";
    ss << "  \"stream\": {\n";
    ss << "    \"width\": " << width << ",\n";
    ss << "    \"height\": " << height << ",\n";
    ss << "    \"target_fps\": " << targetFps << ",\n";
    ss << "    \"actual_fps\": " << actualFps << ",\n";
    ss << "    \"avg_bitrate_kbps\": " << averageBitrateKbps << "\n";
    ss << "  },\n";
    ss << "  \"counters\": {\n";
    ss << "    \"access_units_ingested\": " << accessUnitsIngested << ",\n";
    ss << "    \"frames_decoded\": " << framesDecoded << ",\n";
    ss << "    \"frames_presented\": " << framesPresented << ",\n";
    ss << "    \"frames_dropped\": " << framesDropped << ",\n";
    ss << "    \"decode_errors\": " << decodeErrors << "\n";
    ss << "  },\n";
    ss << "  \"latencies_us\": {\n";
    ss << "    \"decode\": {\n";
    ss << "      \"min\": " << decodeLatencyUs.minUs << ", \"max\": " << decodeLatencyUs.maxUs
       << ", \"avg\": " << decodeLatencyUs.avgUs << ", \"p50\": " << decodeLatencyUs.p50Us
       << ", \"p95\": " << decodeLatencyUs.p95Us << ", \"p99\": " << decodeLatencyUs.p99Us << "\n";
    ss << "    },\n";
    ss << "    \"present\": {\n";
    ss << "      \"min\": " << presentLatencyUs.minUs << ", \"max\": " << presentLatencyUs.maxUs
       << ", \"avg\": " << presentLatencyUs.avgUs << ", \"p50\": " << presentLatencyUs.p50Us
       << ", \"p95\": " << presentLatencyUs.p95Us << ", \"p99\": " << presentLatencyUs.p99Us << "\n";
    ss << "    },\n";
    ss << "    \"decode_to_present\": {\n";
    ss << "      \"min\": " << decodeToPresentLatencyUs.minUs << ", \"max\": " << decodeToPresentLatencyUs.maxUs
       << ", \"avg\": " << decodeToPresentLatencyUs.avgUs << ", \"p50\": " << decodeToPresentLatencyUs.p50Us
       << ", \"p95\": " << decodeToPresentLatencyUs.p95Us << ", \"p99\": " << decodeToPresentLatencyUs.p99Us << "\n";
    ss << "    }\n";
    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

DecodePresentPipeline::DecodePresentPipeline() noexcept {
    auto& reg = telemetry::MetricRegistry::instance();
    histDecodeLatency_ = reg.getOrCreateHistogram("pipeline_decode_latency_us");
    histPresentLatency_ = reg.getOrCreateHistogram("pipeline_present_latency_us");
    histDecodeToPresent_ = reg.getOrCreateHistogram("pipeline_decode_to_present_us");
}

DecodePresentPipeline::~DecodePresentPipeline() {
    (void)stop();
}

common::Result<void> DecodePresentPipeline::initialize(
    const DecodeConfig& decConfig,
    const render::RenderConfig& rndConfig
) {
    decConfig_ = decConfig;
    rndConfig_ = rndConfig;

    accessUnitsIngested_ = 0;
    framesDecoded_ = 0;
    framesPresented_ = 0;
    framesDropped_ = 0;
    decodeErrors_ = 0;
    totalBytesIngested_ = 0;

    histDecodeLatency_->reset();
    histPresentLatency_->reset();
    histDecodeToPresent_->reset();

#if defined(_WIN32)
    auto devRes = platform::windows::D3D11DeviceManager::createDevice(0);
    if (!devRes) {
        return devRes.error();
    }
    auto sharedDevice = devRes.value().first;
    auto sharedContext = devRes.value().second;

    auto decRes = D3D11VideoDecoder::create();
    if (!decRes) return decRes.error();
    decoder_ = std::move(decRes.value());

    auto decInitRes = decoder_->initializeWithDevice(decConfig_, sharedDevice, sharedContext);
    if (!decInitRes) return decInitRes;

    auto rndRes = render::D3D11Renderer::create();
    if (!rndRes) return rndRes.error();
    renderer_ = std::move(rndRes.value());

    auto rndInitRes = renderer_->initializeWithDevice(rndConfig_, sharedDevice, sharedContext);
    if (!rndInitRes) return rndInitRes;

    MIST_LOG_INFO(common::SubsystemId::Decode, "DecodePresentPipeline initialized successfully.");
    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotImplemented, "Only supported on Windows");
#endif
}

common::Result<DecodeBenchmarkReport> DecodePresentPipeline::runBitstream(
    const std::string& h264FilePath,
    uint32_t targetFps,
    uint32_t maxDurationSeconds
) {
    if (!decoder_ || !renderer_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotInitialized, "Pipeline not initialized");
    }

    AnnexBReader reader;
    auto readRes = reader.openFile(h264FilePath, targetFps);
    if (!readRes) {
        return readRes.error();
    }

    stopRequested_.store(false);
    isRunning_.store(true);
    pipelineTimer_.reset();

    const auto& aus = reader.getAccessUnits();
    const uint32_t frameIntervalMs = (targetFps > 0) ? (1000 / targetFps) : 16;
    const auto startTime = std::chrono::steady_clock::now();

    platform::windows::HighResolutionTimer qpc;

    for (size_t i = 0; i < aus.size() && !stopRequested_.load(); ++i) {
        if (maxDurationSeconds > 0) {
            auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count();
            if (elapsedSec >= maxDurationSeconds) break;
        }

        const auto& au = aus[i];
        accessUnitsIngested_.fetch_add(1, std::memory_order_relaxed);
        totalBytesIngested_.fetch_add(au.data.size(), std::memory_order_relaxed);

        encode::EncodedPacket packet{
            .data = au.data,
            .isKeyframe = au.isKeyframe,
            .timestampNs = pipelineTimer_.elapsedNanoseconds(),
            .frameNumber = au.sequenceNumber
        };

        qpc.reset();
        auto subRes = decoder_->submitPacket(packet);
        if (!subRes) {
            decodeErrors_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        double decodeDurationUs = qpc.elapsedMicroseconds();

        // Retrieve decoded frames
        while (true) {
            auto frameRes = decoder_->getNextFrame();
            if (!frameRes) break;

            const auto& frame = frameRes.value();
            framesDecoded_.fetch_add(1, std::memory_order_relaxed);
            histDecodeLatency_->record(decodeDurationUs);

            qpc.reset();
            auto presRes = renderer_->present(frame);
            if (presRes) {
                framesPresented_.fetch_add(1, std::memory_order_relaxed);
                double presDurationUs = qpc.elapsedMicroseconds();
                histPresentLatency_->record(presDurationUs);

                uint64_t nowNs = pipelineTimer_.elapsedNanoseconds();
                if (nowNs >= frame.timestampNs) {
                    double totalUs = static_cast<double>(nowNs - frame.timestampNs) / 1000.0;
                    histDecodeToPresent_->record(totalUs);
                }
            } else {
                framesDropped_.fetch_add(1, std::memory_order_relaxed);
            }

            (void)decoder_->releaseFrame(frame);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(frameIntervalMs));
    }

    (void)stop();
    return generateReport();
}

common::Result<void> DecodePresentPipeline::stop() {
    stopRequested_.store(true);
    if (!isRunning_.exchange(false)) {
        return common::Result<void>::success();
    }

    if (decoder_) {
        (void)decoder_->flush();
    }
    if (renderer_) {
        (void)renderer_->shutdown();
    }

    MIST_LOG_INFO(common::SubsystemId::Decode, "DecodePresentPipeline stopped.");
    return common::Result<void>::success();
}

DecodeBenchmarkReport DecodePresentPipeline::generateReport() const {
    double elapsedSec = pipelineTimer_.elapsedMilliseconds() / 1000.0;
    if (elapsedSec <= 0.0) elapsedSec = 1.0;

    uint64_t presented = framesPresented_.load();
    double actualFps = static_cast<double>(presented) / elapsedSec;
    double avgBitrateKbps = (static_cast<double>(totalBytesIngested_.load() * 8) / elapsedSec) / 1000.0;

    std::string gpuName = "NVIDIA GeForce";
    auto adapters = platform::windows::D3D11DeviceManager::enumerateAdapters();
    if (!adapters.empty()) {
        gpuName = adapters[0].description;
    }

    return DecodeBenchmarkReport{
        .hardwareGpu = gpuName,
        .decoderBackend = "Direct3D 11 Hardware Video Decoder (D3D11VA)",
        .width = rndConfig_.width,
        .height = rndConfig_.height,
        .targetFps = 60,
        .actualFps = actualFps,
        .averageBitrateKbps = avgBitrateKbps,
        .accessUnitsIngested = accessUnitsIngested_.load(),
        .framesDecoded = framesDecoded_.load(),
        .framesPresented = presented,
        .framesDropped = framesDropped_.load(),
        .decodeErrors = decodeErrors_.load(),
        .decodeLatencyUs = histDecodeLatency_->snapshot(),
        .presentLatencyUs = histPresentLatency_->snapshot(),
        .decodeToPresentLatencyUs = histDecodeToPresent_->snapshot()
    };
}

} // namespace mist::decode
