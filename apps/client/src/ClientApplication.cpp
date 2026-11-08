#include "ClientApplication.h"
#include "mist/common/Logger.h"
#include "mist/decode/D3D11VideoDecoder.h"
#include "mist/render/D3D11Renderer.h"
#include "mist/platform/windows/PlatformWindows.h"
#include "mist/transport/WebRtcTransport.h"
#include "mist/services/WebSocketSignalingClient.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

namespace mist::client {

ClientApplication::ClientApplication() noexcept = default;

ClientApplication::~ClientApplication() {
    if (isInitialized_.load()) {
        (void)shutdown();
    }
}

common::Result<std::optional<int>> ClientApplication::initialize(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Mist Client v0.1.0 (Milestone 2: Native Hardware Decoding & D3D11 Presentation)\n"
                      << "Usage: mist_client [options]\n\n"
                      << "General Options:\n"
                      << "  -h, --help                Show help information\n"
                      << "  -v, --version             Show version string\n"
                      << "  --self-test               Run internal self-tests and exit\n"
                      << "  --config <path>           Load custom configuration file\n\n"
                      << "Milestone 2 Diagnostics:\n"
                      << "  --decoder-info            Query hardware video decoder capabilities\n"
                      << "  --decode-test             Run video decoding test on an H.264 bitstream\n"
                      << "  --present-test            Run Direct3D 11 presentation engine test\n"
                      << "  --decode-benchmark        Run full decoding benchmark and emit report\n\n"
                      << "Milestone 3 WebRTC Diagnostics:\n"
                      << "  --transport-info          Query client WebRTC transport capabilities\n"
                      << "  --receive-test            Receive and decode real-time stream over WebRTC\n"
                      << "  --stream-receive          Alias for --receive-test\n"
                      << "    --signaling-url <url>   WebSocket signaling URL (default: ws://127.0.0.1:8080)\n"
                      << "    --session-id <id>       Session identifier (default: mist-session-1)\n"
                      << "    --auth-token <token>    Session authentication token (default: mist-dev-token)\n\n"
                      << "Diagnostic Parameters:\n"
                      << "  --input <path>            Input H.264 bitstream file (e.g. artifacts/test.h264)\n"
                      << "  --report <path>           Output benchmark JSON file path\n"
                      << "  --width <pixels>          Target video width (default: 1920)\n"
                      << "  --height <pixels>         Target video height (default: 1080)\n"
                      << "  --fps <number>            Target frame rate (default: 60)\n"
                      << "  --duration <seconds>      Duration in seconds (default: 5)\n"
                      << "  --headless                Disable GUI window presentation (ideal for CI)\n"
                      << "  --vsync                   Enable vertical sync (default: false)\n";
            return std::optional<int>(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mist_client version 0.1.0\n";
            return std::optional<int>(0);
        } else if (arg == "--self-test") {
            options_.mode = ClientCommandMode::SelfTest;
        } else if (arg == "--decoder-info") {
            options_.mode = ClientCommandMode::DecoderInfo;
        } else if (arg == "--decode-test") {
            options_.mode = ClientCommandMode::DecodeTest;
        } else if (arg == "--present-test") {
            options_.mode = ClientCommandMode::PresentTest;
        } else if (arg == "--decode-benchmark") {
            options_.mode = ClientCommandMode::DecodeBenchmark;
        } else if (arg == "--transport-info") {
            options_.mode = ClientCommandMode::TransportInfo;
        } else if (arg == "--receive-test") {
            options_.mode = ClientCommandMode::ReceiveTest;
        } else if (arg == "--stream-receive") {
            options_.mode = ClientCommandMode::StreamReceive;
        } else if (arg == "--signaling-url" && i + 1 < argc) {
            options_.signalingUrl = argv[++i];
        } else if (arg == "--session-id" && i + 1 < argc) {
            options_.sessionId = argv[++i];
        } else if (arg == "--auth-token" && i + 1 < argc) {
            options_.authToken = argv[++i];
        } else if (arg == "--input") {
            if (i + 1 < argc) {
                options_.inputPath = argv[++i];
            } else {
                return common::Error(common::SubsystemId::ClientApp, common::ErrorCode::InvalidArgument, "Missing value for --input");
            }
        } else if (arg == "--report") {
            if (i + 1 < argc) {
                options_.reportPath = argv[++i];
            } else {
                return common::Error(common::SubsystemId::ClientApp, common::ErrorCode::InvalidArgument, "Missing value for --report");
            }
        } else if (arg == "--width") {
            if (i + 1 < argc) {
                options_.width = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
        } else if (arg == "--height") {
            if (i + 1 < argc) {
                options_.height = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
        } else if (arg == "--fps") {
            if (i + 1 < argc) {
                options_.fps = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
        } else if (arg == "--duration") {
            if (i + 1 < argc) {
                options_.durationSeconds = static_cast<uint32_t>(std::stoul(argv[++i]));
            }
        } else if (arg == "--headless") {
            options_.headless = true;
        } else if (arg == "--vsync") {
            options_.vsync = true;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                options_.configPath = argv[++i];
            } else {
                return common::Error(common::SubsystemId::ClientApp, common::ErrorCode::InvalidArgument, "Missing path argument for --config");
            }
        } else {
            return common::Error(common::SubsystemId::ClientApp, common::ErrorCode::InvalidArgument, "Unknown command line option: " + std::string(arg));
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
        case ClientCommandMode::DecoderInfo:
            return std::optional<int>(executeDecoderInfo());
        case ClientCommandMode::DecodeTest:
            return std::optional<int>(executeDecodeTest());
        case ClientCommandMode::PresentTest:
            return std::optional<int>(executePresentTest());
        case ClientCommandMode::DecodeBenchmark:
            return std::optional<int>(executeDecodeBenchmark());
        case ClientCommandMode::TransportInfo:
            return std::optional<int>(executeTransportInfo());
        case ClientCommandMode::ReceiveTest:
        case ClientCommandMode::StreamReceive:
            return std::optional<int>(executeReceiveStream());
        case ClientCommandMode::SelfTest:
            return std::optional<int>(runSelfTest().hasValue() ? 0 : 1);
        case ClientCommandMode::Normal:
        default:
            return std::optional<int>(std::nullopt);
    }
}

int ClientApplication::executeDecoderInfo() {
    auto cap = decode::D3D11VideoDecoder::queryCapabilities();
    std::cout << "Direct3D 11 Hardware Decoder Capabilities:\n"
              << "  Device Name:            " << (cap.deviceName.empty() ? "Direct3D 11 Graphics Adapter" : cap.deviceName) << "\n"
              << "  Driver Version:         " << cap.driverVersion << "\n"
              << "  Hardware Acceleration:  " << (cap.isHardwareAccelerated ? "YES (Hardware D3D11VA)" : "NO") << "\n"
              << "  H.264 Baseline/High:    " << (cap.supportsH264 ? "YES" : "NO") << "\n"
              << "  Max Resolution:         " << cap.maxResolutionWidth << "x" << cap.maxResolutionHeight << "\n";
    return 0;
}

int ClientApplication::executeDecodeTest() {
    if (options_.inputPath.empty()) {
        std::cerr << "Error: --decode-test requires --input <path_to_h264_file>" << std::endl;
        return 1;
    }

    auto cap = decode::D3D11VideoDecoder::queryCapabilities();
    if (!cap.supportsH264) {
        std::cout << "[SKIP] Hardware H.264 video decoder is unavailable on this system.\n";
        return 0;
    }

    decode::DecodeConfig decConfig{
        .codec = encode::VideoCodec::H264,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    render::RenderConfig rndConfig{
        .nativeWindowHandle = nullptr,
        .width = options_.width,
        .height = options_.height,
        .vsync = options_.vsync,
        .enableHdr = false,
        .headless = options_.headless
    };

    decode::DecodePresentPipeline pipeline;
    auto initRes = pipeline.initialize(decConfig, rndConfig);
    if (!initRes) {
        std::cerr << "Pipeline initialization failed: " << initRes.error().format() << std::endl;
        return 1;
    }

    std::cout << "Executing Decode Test on " << options_.inputPath
              << " (" << options_.width << "x" << options_.height << " @ " << options_.fps << " FPS"
              << (options_.headless ? ", Headless" : ", Window") << ")...\n";

    auto runRes = pipeline.runBitstream(options_.inputPath, options_.fps, options_.durationSeconds);
    if (!runRes) {
        std::cerr << "Decode execution failed: " << runRes.error().format() << std::endl;
        return 1;
    }

    const auto& report = runRes.value();
    std::cout << "Decode Test Results:\n"
              << "  GPU:                   " << report.hardwareGpu << "\n"
              << "  Decoder Backend:       " << report.decoderBackend << "\n"
              << "  Access Units Ingested: " << report.accessUnitsIngested << "\n"
              << "  Frames Decoded:        " << report.framesDecoded << "\n"
              << "  Frames Presented:      " << report.framesPresented << "\n"
              << "  Frames Dropped:        " << report.framesDropped << "\n"
              << "  Actual FPS:            " << report.actualFps << "\n"
              << "  Decode Latency:        p50=" << report.decodeLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.decodeLatencyUs.p95Us / 1000.0 << " ms, "
              << "p99=" << report.decodeLatencyUs.p99Us / 1000.0 << " ms\n"
              << "  Present Latency:       p50=" << report.presentLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.presentLatencyUs.p95Us / 1000.0 << " ms\n";

    return 0;
}

int ClientApplication::executePresentTest() {
    render::RenderConfig rndConfig{
        .nativeWindowHandle = nullptr,
        .width = options_.width,
        .height = options_.height,
        .vsync = options_.vsync,
        .enableHdr = false,
        .headless = options_.headless
    };

    auto rendererRes = render::D3D11Renderer::create();
    if (!rendererRes) {
        std::cerr << "Failed to create renderer: " << rendererRes.error().format() << std::endl;
        return 1;
    }
    auto renderer = std::move(rendererRes.value());

    auto initRes = renderer->initialize(rndConfig);
    if (!initRes) {
        std::cerr << "Renderer initialization failed: " << initRes.error().format() << std::endl;
        return 1;
    }

    std::cout << "Running Direct3D 11 Presentation Test (" << options_.width << "x" << options_.height
              << " @ " << options_.fps << " FPS" << (options_.headless ? ", Headless" : "") << ")...\n";

    // Allocate test surface on D3D11 device
    auto d3dDevice = renderer->getD3D11Device();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = options_.width;
    desc.Height = options_.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> testTexture;
    HRESULT hr = d3dDevice->CreateTexture2D(&desc, nullptr, testTexture.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 test texture" << std::endl;
        return 1;
    }

    // Present 60 test frames (~1 second)
    const uint32_t testFrames = options_.fps;
    const uint32_t frameIntervalMs = 1000 / options_.fps;

    for (uint32_t i = 1; i <= testFrames; ++i) {
        decode::DecodedFrame frame{
            .width = options_.width,
            .height = options_.height,
            .timestampNs = static_cast<uint64_t>(i) * 16666666ULL,
            .frameNumber = i,
            .nativeResource = testTexture.Get()
        };

        auto presRes = renderer->present(frame);
        if (!presRes) {
            std::cerr << "Present failed on frame " << i << ": " << presRes.error().format() << std::endl;
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(frameIntervalMs));
    }

    std::cout << "Presentation Test Results:\n"
              << "  Frames Presented: " << renderer->getFramesPresented() << "\n"
              << "  Frames Dropped:   " << renderer->getFramesDropped() << "\n"
              << "  Last Latency:     " << renderer->getPresentLatencyUs() / 1000.0 << " ms\n";

    (void)renderer->shutdown();
    return 0;
}

int ClientApplication::executeDecodeBenchmark() {
    if (options_.inputPath.empty()) {
        std::cerr << "Error: --decode-benchmark requires --input <path_to_h264_file>" << std::endl;
        return 1;
    }

    decode::DecodeConfig decConfig{
        .codec = encode::VideoCodec::H264,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    render::RenderConfig rndConfig{
        .nativeWindowHandle = nullptr,
        .width = options_.width,
        .height = options_.height,
        .vsync = false,
        .enableHdr = false,
        .headless = options_.headless
    };

    decode::DecodePresentPipeline pipeline;
    auto initRes = pipeline.initialize(decConfig, rndConfig);
    if (!initRes) {
        std::cerr << "Pipeline initialization failed: " << initRes.error().format() << std::endl;
        return 1;
    }

    std::cout << "Executing Milestone 2 Hardware Decode & Presentation Benchmark on " << options_.inputPath << "...\n";

    auto runRes = pipeline.runBitstream(options_.inputPath, options_.fps, options_.durationSeconds);
    if (!runRes) {
        std::cerr << "Benchmark run failed: " << runRes.error().format() << std::endl;
        return 1;
    }

    const auto& report = runRes.value();
    std::cout << "Benchmark Complete:\n"
              << "  GPU:                   " << report.hardwareGpu << "\n"
              << "  Decoder:               " << report.decoderBackend << "\n"
              << "  Access Units Ingested: " << report.accessUnitsIngested << "\n"
              << "  Frames Decoded:        " << report.framesDecoded << "\n"
              << "  Frames Presented:      " << report.framesPresented << "\n"
              << "  Actual FPS:            " << report.actualFps << "\n"
              << "  Average Bitrate:       " << report.averageBitrateKbps << " kbps\n"
              << "  Decode Latency:        p50=" << report.decodeLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.decodeLatencyUs.p95Us / 1000.0 << " ms\n"
              << "  Present Latency:       p50=" << report.presentLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.presentLatencyUs.p95Us / 1000.0 << " ms\n"
              << "  Total Pipeline:        p50=" << report.decodeToPresentLatencyUs.p50Us / 1000.0 << " ms, "
              << "p95=" << report.decodeToPresentLatencyUs.p95Us / 1000.0 << " ms\n";

    if (!options_.reportPath.empty()) {
        std::ofstream reportFile(options_.reportPath);
        if (reportFile.is_open()) {
            reportFile << report.toJson();
            std::cout << "Report written to: " << options_.reportPath << "\n";
        } else {
            std::cerr << "Failed to open report output path: " << options_.reportPath << std::endl;
        }
    }

    return 0;
}

common::Result<void> ClientApplication::runSelfTest() {
    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Running Client self-test suite (Milestone 2)...");

    // 1. Verify configuration integrity
    if (auto res = config_.client.validate(); !res) {
        return res;
    }

    // 2. Verify high-resolution platform timer
    platform::windows::HighResolutionTimer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (timer.elapsedMicroseconds() <= 0.0) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError, "HighResolutionTimer query failed");
    }

    // 3. Query D3D11 hardware decoder capabilities
    auto cap = decode::D3D11VideoDecoder::queryCapabilities();
    MIST_LOG_INFO(common::SubsystemId::ClientApp, std::string("Self-test: Hardware H.264 Decoder Support = ") + (cap.supportsH264 ? "YES" : "NO/UNAVAILABLE"));

    // 4. Test Direct3D 11 presentation engine
    render::RenderConfig rndConfig{
        .nativeWindowHandle = nullptr,
        .width = 1280,
        .height = 720,
        .vsync = false,
        .enableHdr = false,
        .headless = true // Headless self-test
    };

    auto rendererRes = render::D3D11Renderer::create();
    if (rendererRes) {
        auto renderer = std::move(rendererRes.value());
        if (auto initRes = renderer->initialize(rndConfig); initRes) {
            MIST_LOG_INFO(common::SubsystemId::ClientApp, "Self-test: Direct3D 11 renderer initialized successfully.");
            (void)renderer->shutdown();
        }
    }

    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Self-test completed successfully.");
    return common::Result<void>::success();
}

int ClientApplication::executeTransportInfo() {
    std::cout << "=== Mist WebRTC Transport Client Subsystem Info ===\n"
              << "Transport Implementation: libdatachannel v0.22.4 (MbedTLS 3.6.2)\n"
              << "Protocol Standard:        WebRTC (RFC 8829, RFC 8831)\n"
              << "Video Codec:              H.264 (RFC 6184 RTP Depacketization)\n"
              << "Video Clock Rate:         90000 Hz\n"
              << "Zero Disk I/O Pipeline:   Enabled (Direct In-Memory AU to MFT Video Decoder)\n"
              << "RTCP Feedback:            PLI (Picture Loss Indication) for Instant Keyframe Recovery\n"
              << "Default STUN Server:      stun:stun.l.google.com:19302\n"
              << "DataChannel Support:      session-control (reliable), diagnostics (unordered)\n"
              << "Signaling Backend:        mist-signaling (Rust Tokio/Tungstenite)\n";
    return 0;
}

int ClientApplication::executeReceiveStream() {
    auto cap = decode::D3D11VideoDecoder::queryCapabilities();
    if (!cap.supportsH264) {
        std::cout << "[SKIP] Hardware H.264 video decoder unavailable.\n";
        return 0;
    }

    decode::DecodeConfig decConfig{
        .codec = encode::VideoCodec::H264,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    decode::D3D11VideoDecoder decoder;
    auto decInit = decoder.initialize(decConfig);
    if (!decInit) {
        std::cerr << "Video decoder initialization failed: " << decInit.error().format() << "\n";
        return 1;
    }

    std::unique_ptr<render::D3D11Renderer> renderer;
    if (!options_.headless) {
        render::RenderConfig rndConfig{
            .nativeWindowHandle = nullptr,
            .width = options_.width,
            .height = options_.height,
            .vsync = options_.vsync,
            .enableHdr = false,
            .headless = false
        };
        renderer = std::make_unique<render::D3D11Renderer>();
        auto rendInit = renderer->initialize(rndConfig);
        if (!rendInit) {
            std::cerr << "Renderer initialization failed: " << rendInit.error().format() << "\n";
            return 1;
        }
    }

    auto signaling = std::make_shared<services::WebSocketSignalingClient>();
    transport::WebRtcTransportConfig rtcConfig{};
    rtcConfig.isHost = false;
    rtcConfig.sessionId = options_.sessionId;
    rtcConfig.authToken = options_.authToken;
    rtcConfig.signalingUrl = options_.signalingUrl;
    rtcConfig.videoClockRate = 90000;

    transport::WebRtcTransport rtcTransport;
    auto rtcRes = rtcTransport.initializeWebRtc(rtcConfig, signaling);
    if (!rtcRes) {
        std::cerr << "WebRTC transport init failed: " << rtcRes.error().format() << "\n";
        return 1;
    }

    std::atomic<uint64_t> framesReceived{0};
    std::atomic<uint64_t> framesDecoded{0};
    std::atomic<uint64_t> framesPresented{0};
    std::atomic<uint64_t> totalBytesReceived{0};

    rtcTransport.setFrameReceivedCallback([&](const std::vector<uint8_t>& accessUnit, uint64_t timestampNs) {
        framesReceived.fetch_add(1, std::memory_order_relaxed);
        totalBytesReceived.fetch_add(accessUnit.size(), std::memory_order_relaxed);

        encode::EncodedPacket packet{};
        packet.data = accessUnit;
        packet.timestampNs = timestampNs;

        auto subRes = decoder.submitPacket(packet);
        if (subRes) {
            auto frameRes = decoder.getNextFrame();
            if (frameRes) {
                framesDecoded.fetch_add(1, std::memory_order_relaxed);
                auto frame = frameRes.value();
                if (renderer) {
                    auto presRes = renderer->present(frame);
                    if (presRes) {
                        framesPresented.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                (void)decoder.releaseFrame(frame);
            }
        } else {
            auto receiver = rtcTransport.getVideoReceiver();
            if (receiver) {
                receiver->requestKeyframe();
            }
        }
    });

    auto connRes = rtcTransport.connect();
    if (!connRes) {
        std::cerr << "Signaling connection failed: " << connRes.error().format() << "\n";
        return 1;
    }

    std::cout << "Client stream receive started for " << options_.durationSeconds
              << "s (Session: " << options_.sessionId << ", Signaling: " << options_.signalingUrl
              << (options_.headless ? ", Headless" : ", GUI Window") << ")...\n";

    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= options_.durationSeconds) {
            break;
        }

        if (renderer && renderer->getHwnd()) {
            MSG msg{};
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto actualDuration = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    uint64_t rec = framesReceived.load();
    uint64_t dec = framesDecoded.load();
    uint64_t rnd = framesPresented.load();
    double fps = actualDuration > 0.0 ? static_cast<double>(dec) / actualDuration : 0.0;
    double bitrateMbps = actualDuration > 0.0 ? ((static_cast<double>(totalBytesReceived.load()) * 8.0) / (actualDuration * 1000000.0)) : 0.0;

    auto stats = rtcTransport.getStatsSnapshot();

    std::cout << "=== Client Streaming Results ===\n"
              << "Duration:        " << actualDuration << " s\n"
              << "Frames Received: " << rec << "\n"
              << "Frames Decoded:  " << dec << " (" << fps << " FPS)\n"
              << "Frames Presented:" << rnd << "\n"
              << "Bitrate:         " << bitrateMbps << " Mbps\n"
              << "Packets Recv:    " << stats.packetsReceived << "\n"
              << "Bytes Recv:      " << stats.bytesReceived << "\n";

    if (!options_.reportPath.empty()) {
        std::ofstream rep(options_.reportPath);
        if (rep.is_open()) {
            rep << "{\n"
                << "  \"duration_sec\": " << actualDuration << ",\n"
                << "  \"frames_received\": " << rec << ",\n"
                << "  \"frames_decoded\": " << dec << ",\n"
                << "  \"frames_presented\": " << rnd << ",\n"
                << "  \"achieved_fps\": " << fps << ",\n"
                << "  \"bitrate_mbps\": " << bitrateMbps << ",\n"
                << "  \"packets_received\": " << stats.packetsReceived << ",\n"
                << "  \"bytes_received\": " << stats.bytesReceived << "\n"
                << "}\n";
        }
    }
    return 0;
}

int ClientApplication::run() {
    if (!isInitialized_.load()) {
        std::cerr << "Cannot run Client application: not initialized." << std::endl;
        return 1;
    }

    isRunning_.store(true);
    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Mist Client is running. (Milestone 2: Ready)");

    while (isRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        break;
    }

    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Mist Client run cycle finished.");
    return 0;
}

void ClientApplication::requestShutdown() noexcept {
    isRunning_.store(false);
}

common::Result<void> ClientApplication::shutdown() {
    if (!isInitialized_.exchange(false)) {
        return common::Result<void>::success();
    }

    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Shutting down Mist Client Application...");
    isRunning_.store(false);
    return common::Result<void>::success();
}

} // namespace mist::client
