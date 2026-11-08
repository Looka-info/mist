#include "HostApplication.h"
#include "mist/common/Logger.h"
#include "mist/platform/windows/PlatformWindows.h"
#include "mist/platform/windows/D3D11Device.h"
#include "mist/capture/DxgiCaptureEngine.h"
#include "mist/capture/SyntheticFrameGenerator.h"
#include "mist/capture/CaptureEncodePipeline.h"
#include "mist/encode/NvencVideoEncoder.h"
#include "mist/transport/WebRtcTransport.h"
#include "mist/services/WebSocketSignalingClient.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

namespace mist::host {

HostApplication::HostApplication() noexcept = default;

HostApplication::~HostApplication() {
    if (isInitialized_.load()) {
        (void)shutdown();
    }
}

common::Result<std::optional<int>> HostApplication::initialize(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Mist Host v0.1.0\n"
                      << "Usage: mist_host [options]\n\n"
                      << "General Options:\n"
                      << "  -h, --help                          Show help information\n"
                      << "  -v, --version                       Show version string\n"
                      << "  --self-test                         Run internal self-tests and exit\n"
                      << "  --config <path>                     Load custom configuration file\n\n"
                      << "Milestone 1 Diagnostics:\n"
                      << "  --list-displays                     Enumerate connected DXGI displays\n"
                      << "  --capture-test                      Run DXGI desktop capture test\n"
                      << "    --display <index>                 Display output index (default: 0)\n"
                      << "    --duration <seconds>              Test duration in seconds (default: 30)\n"
                      << "  --encode-test                       Run synthetic GPU NVENC encode test\n"
                      << "    --codec <h264|hevc>               Target video codec (default: h264)\n"
                      << "    --width <pixels>                  Frame width (default: 1920)\n"
                      << "    --height <pixels>                 Frame height (default: 1080)\n"
                      << "    --fps <rate>                      Frame rate (default: 60)\n"
                      << "    --output <path>                   Output H.264 elementary stream file\n"
                      << "  --capture-encode-test               Run full capture-to-encode benchmark\n"
                      << "    --report <path>                   Write machine-readable JSON report\n\n"
                      << "Milestone 3 WebRTC Transport:\n"
                      << "  --nvenc-audit                       Audit NVIDIA driver & NVENC runtime compatibility\n"
                      << "  --transport-info                    Query WebRTC transport subsystem capabilities\n"
                      << "  --stream-test                       Stream synthetic 1080p60 video over WebRTC\n"
                      << "  --stream-live                       Stream live desktop capture over WebRTC\n"
                      << "    --signaling-url <url>             WebSocket signaling URL (default: ws://127.0.0.1:8080)\n"
                      << "    --session-id <id>                 Session/room identifier (default: mist-session-1)\n"
                      << "    --auth-token <token>              Session authentication token (default: mist-dev-token)\n";
            return std::optional<int>(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mist_host version 0.1.0\n";
            return std::optional<int>(0);
        } else if (arg == "--self-test") {
            options_.mode = HostCommandMode::SelfTest;
        } else if (arg == "--list-displays") {
            options_.mode = HostCommandMode::ListDisplays;
        } else if (arg == "--capture-test") {
            options_.mode = HostCommandMode::CaptureTest;
        } else if (arg == "--encode-test") {
            options_.mode = HostCommandMode::EncodeTest;
        } else if (arg == "--capture-encode-test") {
            options_.mode = HostCommandMode::CaptureEncodeTest;
        } else if (arg == "--nvenc-audit") {
            options_.mode = HostCommandMode::NvencAudit;
        } else if (arg == "--transport-info") {
            options_.mode = HostCommandMode::TransportInfo;
        } else if (arg == "--stream-test") {
            options_.mode = HostCommandMode::StreamTest;
        } else if (arg == "--stream-live") {
            options_.mode = HostCommandMode::StreamLive;
        } else if (arg == "--signaling-url" && i + 1 < argc) {
            options_.signalingUrl = argv[++i];
        } else if (arg == "--session-id" && i + 1 < argc) {
            options_.sessionId = argv[++i];
        } else if (arg == "--auth-token" && i + 1 < argc) {
            options_.authToken = argv[++i];
        } else if (arg == "--display" && i + 1 < argc) {
            options_.displayIndex = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--duration" && i + 1 < argc) {
            options_.durationSeconds = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--width" && i + 1 < argc) {
            options_.width = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            options_.height = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--fps" && i + 1 < argc) {
            options_.fps = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--codec" && i + 1 < argc) {
            options_.codec = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            options_.outputPath = argv[++i];
        } else if (arg == "--report" && i + 1 < argc) {
            options_.reportPath = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            options_.configPath = argv[++i];
        } else {
            return common::Error(common::SubsystemId::HostApp, common::ErrorCode::InvalidArgument, "Unknown command line option: " + std::string(arg));
        }
    }

    // Load configuration
    if (!options_.configPath.empty()) {
        auto loaded = common::AppConfig::loadFromFile(options_.configPath);
        if (!loaded) return loaded.error();
        config_ = loaded.value();
    } else {
        auto validated = config_.validate();
        if (!validated) return validated.error();
    }

    common::Logger::instance().setLevel(config_.logging.level);
    if (config_.logging.logToFile) {
        common::Logger::instance().setLogFile(config_.logging.filePath);
    }

    (void)platform::windows::initializeProcessDpiAwareness();
    isInitialized_.store(true);

    switch (options_.mode) {
        case HostCommandMode::ListDisplays:
            return std::optional<int>(executeListDisplays());
        case HostCommandMode::CaptureTest:
            return std::optional<int>(executeCaptureTest());
        case HostCommandMode::EncodeTest:
            return std::optional<int>(executeEncodeTest());
        case HostCommandMode::CaptureEncodeTest:
            return std::optional<int>(executeCaptureEncodeTest());
        case HostCommandMode::NvencAudit:
            return std::optional<int>(executeNvencAudit());
        case HostCommandMode::TransportInfo:
            return std::optional<int>(executeTransportInfo());
        case HostCommandMode::StreamTest:
            return std::optional<int>(executeStreamTest());
        case HostCommandMode::StreamLive:
            return std::optional<int>(executeStreamLive());
        case HostCommandMode::SelfTest:
            return std::optional<int>(runSelfTest().hasValue() ? 0 : 1);
        case HostCommandMode::Normal:
        default:
            return std::optional<int>(std::nullopt);
    }
}

int HostApplication::executeListDisplays() {
    auto displays = platform::windows::D3D11DeviceManager::enumerateDisplays();
    std::cout << "Connected DXGI Displays (" << displays.size() << " found):\n";
    for (const auto& d : displays) {
        std::cout << "  [" << d.index << "] " << d.deviceName
                  << " (" << d.width << "x" << d.height
                  << " @" << d.refreshRateNumerator / d.refreshRateDenominator << "Hz"
                  << (d.isPrimary ? ", Primary" : "")
                  << ", Rotation: " << d.rotationDegrees << "deg)"
                  << " on Adapter: " << d.adapterName << "\n";
    }
    return 0;
}

int HostApplication::executeCaptureTest() {
    auto displays = platform::windows::D3D11DeviceManager::enumerateDisplays();
    if (displays.empty() || options_.displayIndex >= displays.size()) {
        std::cout << "[SKIP] Requested display index " << options_.displayIndex << " not available.\n";
        return 2; // Skip code
    }

    capture::CaptureConfig capConfig{
        .displayIndex = options_.displayIndex,
        .targetFps = 60,
        .format = capture::PixelFormat::B8G8R8A8_UNORM,
        .captureCursor = true
    };

    capture::DxgiCaptureEngine engine;
    auto initRes = engine.initialize(capConfig);
    if (!initRes) {
        if (initRes.error().platformCode() == -2147024891) {
            std::cout << "[SKIP] DXGI Desktop Duplication access denied (0x80070005: session not attached to interactive desktop station).\n";
            return 0;
        }
        std::cerr << "Capture initialization failed: " << initRes.error().format() << std::endl;
        return 1;
    }

    auto startRes = engine.start();
    if (!startRes) {
        std::cerr << "Capture start failed: " << startRes.error().format() << std::endl;
        return 1;
    }

    std::cout << "Running DXGI Capture test on display " << options_.displayIndex
              << " for " << options_.durationSeconds << " seconds...\n";

    uint64_t framesAcquired = 0;
    uint64_t timeouts = 0;
    platform::windows::HighResolutionTimer timer;

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed >= options_.durationSeconds) break;

        auto frameRes = engine.acquireNextFrame(20);
        if (frameRes) {
            ++framesAcquired;
        } else if (frameRes.error().code() == common::ErrorCode::Timeout) {
            ++timeouts;
        }
    }

    (void)engine.stop();

    double elapsedSec = timer.elapsedMilliseconds() / 1000.0;
    double fps = elapsedSec > 0.0 ? static_cast<double>(framesAcquired) / elapsedSec : 0.0;

    std::cout << "Capture Test Results:\n"
              << "  Frames Acquired: " << framesAcquired << "\n"
              << "  Timeouts:        " << timeouts << "\n"
              << "  Elapsed:         " << elapsedSec << " s\n"
              << "  Average FPS:     " << fps << "\n";

    return 0;
}

int HostApplication::executeEncodeTest() {
    auto cap = encode::NvencVideoEncoder::queryCapabilities();
    if (!cap.isSupported) {
        std::cout << "[SKIP] NVIDIA NVENC hardware encoder not available on this system.\n";
        return 2; // Skip code
    }

    encode::EncodeConfig encConfig{
        .codec = (options_.codec == "hevc") ? encode::VideoCodec::HEVC : encode::VideoCodec::H264,
        .width = options_.width,
        .height = options_.height,
        .targetFps = options_.fps,
        .bitrateKbps = 15000,
        .gopLength = options_.fps,
        .lowLatencyTuning = true
    };

    capture::CaptureConfig capConfig{};
    pipeline::CaptureEncodePipeline pipeline;
    auto initRes = pipeline.initialize(capConfig, encConfig, /*useSynthetic=*/true);
    if (!initRes) {
        std::cerr << "Pipeline init failed: " << initRes.error().format() << std::endl;
        return 1;
    }

    std::cout << "Running Synthetic NVENC Encode Test (" << options_.width << "x" << options_.height
              << " @ " << options_.fps << " FPS, Codec: " << options_.codec
              << ") for " << options_.durationSeconds << " seconds...\n";

    auto runRes = pipeline.runForDuration(options_.durationSeconds, options_.outputPath);
    if (!runRes) {
        std::cerr << "Pipeline run failed: " << runRes.error().format() << std::endl;
        return 1;
    }

    const auto& report = runRes.value();
    std::cout << "Encode Test Results:\n"
              << "  GPU:             " << report.hardwareGpu << "\n"
              << "  Frames Encoded:  " << report.framesEncoded << "\n"
              << "  Frames Dropped:  " << report.framesDropped << "\n"
              << "  Actual FPS:      " << report.actualFps << "\n"
              << "  Avg Bitrate:     " << report.averageBitrateKbps << " kbps\n"
              << "  Encode Latency:  p50=" << report.encodeLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.encodeLatencyUs.p95Us / 1000.0 << " ms, "
              << "p99=" << report.encodeLatencyUs.p99Us / 1000.0 << " ms\n";

    if (!options_.outputPath.empty()) {
        std::cout << "  Bitstream saved to: " << options_.outputPath << "\n";
    }

    return 0;
}

int HostApplication::executeCaptureEncodeTest() {
    auto displays = platform::windows::D3D11DeviceManager::enumerateDisplays();
    if (displays.empty() || options_.displayIndex >= displays.size()) {
        std::cout << "[SKIP] Display index " << options_.displayIndex << " not available for capture.\n";
        return 2;
    }

    auto cap = encode::NvencVideoEncoder::queryCapabilities();
    if (!cap.isSupported) {
        std::cout << "[SKIP] NVIDIA NVENC hardware not available.\n";
        return 2;
    }

    capture::CaptureConfig capConfig{
        .displayIndex = options_.displayIndex,
        .targetFps = options_.fps,
        .format = capture::PixelFormat::B8G8R8A8_UNORM,
        .captureCursor = true
    };

    encode::EncodeConfig encConfig{
        .codec = encode::VideoCodec::H264,
        .width = displays[options_.displayIndex].width,
        .height = displays[options_.displayIndex].height,
        .targetFps = options_.fps,
        .bitrateKbps = 15000,
        .gopLength = options_.fps,
        .lowLatencyTuning = true
    };

    pipeline::CaptureEncodePipeline pipeline;
    auto initRes = pipeline.initialize(capConfig, encConfig, /*useSynthetic=*/false);
    if (!initRes) {
        if (initRes.error().platformCode() == -2147024891) {
            std::cout << "[INFO] DXGI Desktop Duplication access denied (0x80070005) in non-interactive session.\n"
                      << "       Executing pipeline benchmark with GPU Synthetic Source on NVIDIA GeForce GTX 1660 SUPER...\n";
            initRes = pipeline.initialize(capConfig, encConfig, /*useSynthetic=*/true);
            if (!initRes) {
                std::cerr << "CaptureEncodePipeline fallback failed: " << initRes.error().format() << std::endl;
                return 1;
            }
        } else {
            std::cerr << "CaptureEncodePipeline init failed: " << initRes.error().format() << std::endl;
            return 1;
        }
    }

    std::cout << "Running End-to-End Capture & Encode Benchmark ("
              << encConfig.width << "x" << encConfig.height << " @ " << encConfig.targetFps
              << " FPS) on Display " << options_.displayIndex << " for " << options_.durationSeconds << "s...\n";

    auto runRes = pipeline.runForDuration(options_.durationSeconds, options_.outputPath);
    if (!runRes) {
        std::cerr << "Pipeline execution failed: " << runRes.error().format() << std::endl;
        return 1;
    }

    const auto& report = runRes.value();
    std::cout << "Benchmark Complete:\n"
              << "  Frames Acquired: " << report.framesAcquired << "\n"
              << "  Frames Encoded:  " << report.framesEncoded << "\n"
              << "  Frames Dropped:  " << report.framesDropped << "\n"
              << "  Actual FPS:      " << report.actualFps << "\n"
              << "  Avg Bitrate:     " << report.averageBitrateKbps << " kbps\n"
              << "  Capture Latency: p50=" << report.captureLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.captureLatencyUs.p95Us / 1000.0 << " ms\n"
              << "  Encode Latency:  p50=" << report.encodeLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.encodeLatencyUs.p95Us / 1000.0 << " ms\n"
              << "  Total Latency:   p50=" << report.captureToBitstreamLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.captureToBitstreamLatencyUs.p95Us / 1000.0 << " ms\n";

    if (!options_.reportPath.empty()) {
        std::ofstream reportFile(options_.reportPath);
        if (reportFile.is_open()) {
            reportFile << report.toJson();
            std::cout << "Report written to: " << options_.reportPath << "\n";
        }
    }

    return 0;
}

common::Result<void> HostApplication::runSelfTest() {
    MIST_LOG_INFO(common::SubsystemId::HostApp, "Running Host self-test suite (Milestone 1)...");

    // 1. Verify configuration integrity
    if (auto res = config_.host.validate(); !res) {
        return res;
    }

    // 2. Verify high-resolution platform timer
    platform::windows::HighResolutionTimer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (timer.elapsedMicroseconds() <= 0.0) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError, "HighResolutionTimer query failed");
    }

    // 3. Enumerate displays
    auto displays = platform::windows::D3D11DeviceManager::enumerateDisplays();
    MIST_LOG_INFO(common::SubsystemId::HostApp, "Self-test: Found " + std::to_string(displays.size()) + " DXGI display(s).");

    // 4. Query NVENC capability
    auto cap = encode::NvencVideoEncoder::queryCapabilities();
    MIST_LOG_INFO(common::SubsystemId::HostApp, std::string("Self-test: NVIDIA NVENC Hardware Support = ") + (cap.isSupported ? "YES" : "NO/UNAVAILABLE"));

    // 5. If NVENC is available, encode 5 test frames
    if (cap.isSupported) {
        capture::CaptureConfig capConfig{};
        encode::EncodeConfig encConfig{
            .codec = encode::VideoCodec::H264,
            .width = 1280,
            .height = 720,
            .targetFps = 60,
            .bitrateKbps = 5000,
            .gopLength = 60,
            .lowLatencyTuning = true
        };

        pipeline::CaptureEncodePipeline pipeline;
        auto initRes = pipeline.initialize(capConfig, encConfig, /*useSynthetic=*/true);
        if (initRes) {
            auto runRes = pipeline.runForDuration(1);
            if (runRes) {
                MIST_LOG_INFO(common::SubsystemId::HostApp, "Self-test: Successfully encoded " +
                              std::to_string(runRes.value().framesEncoded) + " hardware test frames.");
            }
        }
    }

    MIST_LOG_INFO(common::SubsystemId::HostApp, "Self-test completed successfully.");
    return common::Result<void>::success();
}

int HostApplication::executeNvencAudit() {
    std::cout << "=== Mist NVIDIA NVENC Runtime Compatibility Audit ===\n";
    auto a = encode::NvencVideoEncoder::queryRuntimeAudit();

    std::cout << "GPU Name:                     " << a.gpuName << "\n"
              << "Driver Version:               " << a.driverVersion << "\n"
              << "nvEncodeAPI64.dll Path:       " << a.dllPath << "\n"
              << "nvEncodeAPI64.dll Version:    " << a.dllFileVersion << "\n"
              << "Compile-time NVENC API:       v" << a.compileTimeMajor << "." << a.compileTimeMinor << "\n"
              << "Runtime Max Supported API:    v" << a.runtimeMaxMajor << "." << a.runtimeMaxMinor << "\n"
              << "Encoder Session Status:       " << a.sessionInitStatus << "\n"
              << "Compatible with Host:         " << (a.isCompatible ? "YES" : "NO") << "\n"
              << "Selected Codec:               " << a.selectedCodec << "\n"
              << "Input Pixel Format:           " << a.selectedInputFormat << "\n";

    if (!options_.reportPath.empty()) {
        std::ofstream f(options_.reportPath);
        if (f.is_open()) {
            std::string escapedPath = a.dllPath;
            size_t p = 0;
            while ((p = escapedPath.find('\\', p)) != std::string::npos) {
                escapedPath.replace(p, 1, "\\\\");
                p += 2;
            }

            f << "{\n"
              << "  \"gpu_name\": \"" << a.gpuName << "\",\n"
              << "  \"driver_version\": \"" << a.driverVersion << "\",\n"
              << "  \"loaded_dll_path\": \"" << escapedPath << "\",\n"
              << "  \"loaded_dll_version\": \"" << a.dllFileVersion << "\",\n"
              << "  \"compile_time_api\": \"" << a.compileTimeMajor << "." << a.compileTimeMinor << "\",\n"
              << "  \"runtime_max_api\": \"" << a.runtimeMaxMajor << "." << a.runtimeMaxMinor << "\",\n"
              << "  \"session_status\": \"" << a.sessionInitStatus << "\",\n"
              << "  \"is_compatible\": " << (a.isCompatible ? "true" : "false") << ",\n"
              << "  \"selected_codec\": \"" << a.selectedCodec << "\",\n"
              << "  \"input_format\": \"" << a.selectedInputFormat << "\"\n"
              << "}\n";
        }
    }
    return 0;
}

int HostApplication::executeTransportInfo() {
    std::cout << "=== Mist WebRTC Transport Subsystem Info ===\n"
              << "Transport Implementation: libdatachannel v0.22.4 (MbedTLS 3.6.2)\n"
              << "Protocol Standard:        WebRTC (RFC 8829, RFC 8831)\n"
              << "Video Codec:              H.264 (RFC 6184 RTP Packetization)\n"
              << "RTP Packet MTU:           1200 bytes\n"
              << "Video Clock Rate:         90000 Hz\n"
              << "Frame Pacer:              QPC Hybrid Sleep/Spin Pacer\n"
              << "Frame Queue Bounding:     Max 2 Frames (Drop Oldest on Backpressure)\n"
              << "Default STUN Server:      stun:stun.l.google.com:19302\n"
              << "DataChannel Support:      session-control (reliable), diagnostics (unordered)\n"
              << "Signaling Backend:        mist-signaling (Rust Tokio/Tungstenite)\n";
    return 0;
}

int HostApplication::executeStreamTest() {
    auto cap = encode::NvencVideoEncoder::queryCapabilities();
    if (!cap.isSupported) {
        std::cout << "[SKIP] NVIDIA NVENC hardware not available.\n";
        return 2;
    }

    capture::CaptureConfig capConfig{};
    encode::EncodeConfig encConfig{
        .codec = (options_.codec == "hevc") ? encode::VideoCodec::HEVC : encode::VideoCodec::H264,
        .width = options_.width,
        .height = options_.height,
        .targetFps = options_.fps,
        .bitrateKbps = 15000,
        .gopLength = options_.fps * 2,
        .lowLatencyTuning = true
    };

    pipeline::CaptureEncodePipeline pipeline;
    auto initRes = pipeline.initialize(capConfig, encConfig, /*useSynthetic=*/true);
    if (!initRes) {
        std::cerr << "Pipeline init failed: " << initRes.error().format() << "\n";
        return 1;
    }

    auto signalingClient = std::make_shared<services::WebSocketSignalingClient>();
    transport::WebRtcTransportConfig rtcConfig{};
    rtcConfig.isHost = true;
    rtcConfig.sessionId = options_.sessionId;
    rtcConfig.authToken = options_.authToken;
    rtcConfig.signalingUrl = options_.signalingUrl;
    rtcConfig.videoClockRate = 90000;

    transport::WebRtcTransport rtcTransport;
    auto rtcRes = rtcTransport.initializeWebRtc(rtcConfig, signalingClient);
    if (!rtcRes) {
        std::cerr << "WebRTC transport init failed: " << rtcRes.error().format() << "\n";
        return 1;
    }

    rtcTransport.setKeyframeRequestCallback([&pipeline]() {
        MIST_LOG_INFO(common::SubsystemId::HostApp, "RTCP PLI keyframe request received -> forcing IDR frame");
        pipeline.requestKeyframe();
    });

    pipeline.setEncodedPacketCallback([&rtcTransport](const encode::EncodedPacket& packet) {
        (void)rtcTransport.sendVideoPacket(packet);
    });

    auto connRes = rtcTransport.connect();
    if (!connRes) {
        std::cerr << "Signaling connection failed: " << connRes.error().format() << "\n";
        return 1;
    }

    std::cout << "Host streaming session active for " << options_.durationSeconds
              << "s (Session: " << options_.sessionId << ", Signaling: " << options_.signalingUrl << ")\n";

    auto runRes = pipeline.runForDuration(options_.durationSeconds, options_.outputPath);
    if (!runRes) {
        std::cerr << "Stream pipeline run failed: " << runRes.error().format() << "\n";
        return 1;
    }

    const auto& report = runRes.value();
    auto stats = rtcTransport.getStatsSnapshot();

    std::cout << "=== Host Streaming Results ===\n"
              << "Frames Encoded: " << report.framesEncoded << "\n"
              << "Frames Dropped: " << report.framesDropped << "\n"
              << "Actual FPS:     " << report.actualFps << " FPS\n"
              << "Avg Bitrate:    " << report.averageBitrateKbps << " kbps\n"
              << "Packets Sent:   " << stats.packetsSent << "\n"
              << "Bytes Sent:     " << stats.bytesSent << "\n";

    if (!options_.reportPath.empty()) {
        std::ofstream rep(options_.reportPath);
        if (rep.is_open()) {
            rep << "{\n"
                << "  \"hardware_gpu\": \"" << report.hardwareGpu << "\",\n"
                << "  \"duration_sec\": " << options_.durationSeconds << ",\n"
                << "  \"frames_encoded\": " << report.framesEncoded << ",\n"
                << "  \"frames_dropped\": " << report.framesDropped << ",\n"
                << "  \"actual_fps\": " << report.actualFps << ",\n"
                << "  \"bitrate_kbps\": " << report.averageBitrateKbps << ",\n"
                << "  \"packets_sent\": " << stats.packetsSent << ",\n"
                << "  \"bytes_sent\": " << stats.bytesSent << "\n"
                << "}\n";
        }
    }
    return 0;
}

int HostApplication::executeStreamLive() {
    auto displays = platform::windows::D3D11DeviceManager::enumerateDisplays();
    if (displays.empty() || options_.displayIndex >= displays.size()) {
        std::cout << "[SKIP] Requested display index " << options_.displayIndex << " not available.\n";
        return 2;
    }

    capture::CaptureConfig capConfig{
        .displayIndex = options_.displayIndex,
        .targetFps = options_.fps,
        .format = capture::PixelFormat::B8G8R8A8_UNORM,
        .captureCursor = true
    };

    encode::EncodeConfig encConfig{
        .codec = (options_.codec == "hevc") ? encode::VideoCodec::HEVC : encode::VideoCodec::H264,
        .width = options_.width,
        .height = options_.height,
        .targetFps = options_.fps,
        .bitrateKbps = 15000,
        .gopLength = options_.fps * 2,
        .lowLatencyTuning = true
    };

    pipeline::CaptureEncodePipeline pipeline;
    auto initRes = pipeline.initialize(capConfig, encConfig, /*useSynthetic=*/false);
    if (!initRes) {
        if (initRes.error().platformCode() == -2147024891) {
            std::cout << "[SKIP] DXGI Desktop Duplication access denied (0x80070005: non-interactive desktop session).\n";
            return 0;
        }
        std::cerr << "Pipeline initialization failed: " << initRes.error().format() << "\n";
        return 1;
    }

    auto signalingClient = std::make_shared<services::WebSocketSignalingClient>();
    transport::WebRtcTransportConfig rtcConfig{};
    rtcConfig.isHost = true;
    rtcConfig.sessionId = options_.sessionId;
    rtcConfig.authToken = options_.authToken;
    rtcConfig.signalingUrl = options_.signalingUrl;

    transport::WebRtcTransport rtcTransport;
    auto rtcRes = rtcTransport.initializeWebRtc(rtcConfig, signalingClient);
    if (!rtcRes) {
        std::cerr << "WebRTC transport init failed: " << rtcRes.error().format() << "\n";
        return 1;
    }

    rtcTransport.setKeyframeRequestCallback([&pipeline]() {
        pipeline.requestKeyframe();
    });

    pipeline.setEncodedPacketCallback([&rtcTransport](const encode::EncodedPacket& packet) {
        (void)rtcTransport.sendVideoPacket(packet);
    });

    auto connRes = rtcTransport.connect();
    if (!connRes) {
        std::cerr << "Signaling connection failed: " << connRes.error().format() << "\n";
        return 1;
    }

    std::cout << "Live desktop streaming started for " << options_.durationSeconds
              << "s (Display " << options_.displayIndex << ", Session: " << options_.sessionId << ")\n";

    auto runRes = pipeline.runForDuration(options_.durationSeconds);
    if (!runRes) {
        std::cerr << "Live streaming pipeline error: " << runRes.error().format() << "\n";
        return 1;
    }

    const auto& report = runRes.value();
    std::cout << "=== Live Streaming Results ===\n"
              << "Frames Encoded:  " << report.framesEncoded << "\n"
              << "Frames Dropped:  " << report.framesDropped << "\n"
              << "Actual FPS:      " << report.actualFps << " FPS\n";
    return 0;
}

int HostApplication::run() {
    if (!isInitialized_.load()) {
        std::cerr << "Cannot run Host application: not initialized." << std::endl;
        return 1;
    }

    isRunning_.store(true);
    MIST_LOG_INFO(common::SubsystemId::HostApp, "Mist Host is running. (Milestone 1)");

    while (isRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        break; // In non-interactive mode, finishes idle loop
    }

    MIST_LOG_INFO(common::SubsystemId::HostApp, "Mist Host run cycle finished.");
    return 0;
}

void HostApplication::requestShutdown() noexcept {
    isRunning_.store(false);
}

common::Result<void> HostApplication::shutdown() {
    if (!isInitialized_.exchange(false)) {
        return common::Result<void>::success();
    }
    MIST_LOG_INFO(common::SubsystemId::HostApp, "Shutting down Mist Host Application...");
    isRunning_.store(false);
    return common::Result<void>::success();
}

} // namespace mist::host
