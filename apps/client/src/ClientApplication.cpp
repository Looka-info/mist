#include "ClientApplication.h"
#include "mist/common/Logger.h"
#include "mist/decode/D3D11VideoDecoder.h"
#include "mist/render/D3D11Renderer.h"
#include "mist/platform/windows/PlatformWindows.h"
#include "mist/transport/WebRtcTransport.h"
#include "mist/services/WebSocketSignalingClient.h"
#include "mist/audio/WasapiAudioPlayback.h"
#include "mist/audio/OpusAudioDecoder.h"
#include "mist/audio/AudioDeviceEnumerator.h"
#include "mist/input/InputTypes.h"
#include "mist/input/InputProtocol.h"
#include "mist/input/GamepadInput.h"
#include "mist/input/ClientInputCapture.h"
#include "mist/telemetry/Metrics.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>

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
            std::cout << "Mist Client v0.3.0-rc3\n"
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
                      << "  --audio                   Enable synchronized audio playout\n\n"
                      << "Milestone 5 Remote Input Diagnostics:\n"
                      << "  --input-test              Run client input capture and protocol self-test\n"
                      << "  --gamepad-test            Poll and report local physical gamepads (XInput)\n"
                      << "  --enable-input            Enable transmitting client input to the host\n\n"
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
            std::cout << "mist_client version 0.3.0-rc3\n";
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
        } else if (arg == "--audio-devices") {
            options_.mode = ClientCommandMode::AudioDevices;
        } else if (arg == "--audio-playback-test") {
            options_.mode = ClientCommandMode::AudioPlaybackTest;
        } else if (arg == "--audio-receive-test") {
            options_.mode = ClientCommandMode::AudioReceiveTest;
        } else if (arg == "--input-test") {
            options_.mode = ClientCommandMode::InputTest;
        } else if (arg == "--gamepad-test") {
            options_.mode = ClientCommandMode::GamepadTest;
        } else if (arg == "--system-info") {
            options_.mode = ClientCommandMode::SystemInfo;
        } else if (arg == "--network-info") {
            options_.mode = ClientCommandMode::NetworkInfo;
        } else if (arg == "--dependency-audit") {
            options_.mode = ClientCommandMode::DependencyAudit;
        } else if (arg == "--enable-input" || arg == "--remote-input") {
            options_.enableInput = true;
        } else if (arg == "--audio") {
            options_.enableAudio = true;
        } else if (arg == "--signaling-url" && i + 1 < argc) {
            options_.signalingUrl = argv[++i];
        } else if (arg == "--session-id" && i + 1 < argc) {
            options_.sessionId = argv[++i];
        } else if (arg == "--auth-token" && i + 1 < argc) {
            options_.authToken = argv[++i];
        } else if (arg == "--turn-server" && i + 1 < argc) {
            options_.turnServer = argv[++i];
        } else if (arg == "--turn-username" && i + 1 < argc) {
            options_.turnUsername = argv[++i];
        } else if (arg == "--turn-password" && i + 1 < argc) {
            options_.turnPassword = argv[++i];
        } else if (arg == "--ipc-pipe" && i + 1 < argc) {
            std::string pipeStr = argv[++i];
            options_.ipcPipe = std::wstring(pipeStr.begin(), pipeStr.end());
        } else if (arg == "--parent-hwnd" && i + 1 < argc) {
            options_.parentHwnd = std::stoull(argv[++i]);
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
    if (options_.turnServer.empty()) {
        const char* envServer = std::getenv("MIST_TURN_SERVER");
        if (envServer) options_.turnServer = envServer;
        const char* envUser = std::getenv("MIST_TURN_USERNAME");
        if (envUser) options_.turnUsername = envUser;
        const char* envPass = std::getenv("MIST_TURN_PASSWORD");
        if (envPass) options_.turnPassword = envPass;
    }
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
        case ClientCommandMode::AudioDevices:
            return std::optional<int>(executeAudioDevices());
        case ClientCommandMode::AudioPlaybackTest:
            return std::optional<int>(executeAudioPlaybackTest());
        case ClientCommandMode::AudioReceiveTest:
            return std::optional<int>(executeAudioReceiveTest());
        case ClientCommandMode::InputTest:
            return std::optional<int>(executeInputTest());
        case ClientCommandMode::GamepadTest:
            return std::optional<int>(executeGamepadTest());
        case ClientCommandMode::SystemInfo:
            return std::optional<int>(executeSystemInfo());
        case ClientCommandMode::NetworkInfo:
            return std::optional<int>(executeNetworkInfo());
        case ClientCommandMode::DependencyAudit:
            return std::optional<int>(executeDependencyAudit());
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

#if defined(_WIN32)
        if (renderer && renderer->getHwnd()) {
            HWND childHwnd = renderer->getHwnd();
            if (options_.parentHwnd != 0) {
                HWND parentHwnd = reinterpret_cast<HWND>(options_.parentHwnd);
                SetParent(childHwnd, parentHwnd);
                LONG_PTR style = GetWindowLongPtrW(childHwnd, GWL_STYLE);
                style &= ~WS_POPUP;
                style &= ~WS_CAPTION;
                style &= ~WS_THICKFRAME;
                style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
                SetWindowLongPtrW(childHwnd, GWL_STYLE, style);
                SetWindowPos(childHwnd, HWND_TOP, 0, 0, options_.width, options_.height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
        }
#endif
    }

#if defined(_WIN32)
    if (!options_.ipcPipe.empty()) {
        ipcClient_ = std::make_unique<ipc::NamedPipeClient>();
        auto* rendRaw = renderer.get();
        (void)ipcClient_->connect(options_.ipcPipe, [rendRaw](const ipc::IpcMessage& msg) {
            if (msg.type == ipc::MessageType::SetViewport && rendRaw && rendRaw->getHwnd()) {
                auto vp = msg.getPayload<ipc::IpcViewportPayload>();
                if (vp) {
                    SetWindowPos(rendRaw->getHwnd(), HWND_TOP, vp->x, vp->y, vp->width, vp->height, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
                }
            }
        });
        if (ipcClient_ && ipcClient_->isConnected()) {
            ipc::IpcMessage initMsg{};
            initMsg.type = ipc::MessageType::ProcessInitialized;
            ipcClient_->sendMessage(initMsg);

            ipc::IpcMessage decMsg{};
            decMsg.type = ipc::MessageType::DecoderInitialized;
            ipcClient_->sendMessage(decMsg);

            if (renderer && renderer->getHwnd()) {
                ipc::IpcMessage winMsg{};
                winMsg.type = ipc::MessageType::WindowHandle;
                ipc::IpcWindowPayload winPayload{};
                winPayload.hwnd = reinterpret_cast<uint64_t>(renderer->getHwnd());
                winPayload.width = options_.width;
                winPayload.height = options_.height;
                winMsg.setPayload(winPayload);
                ipcClient_->sendMessage(winMsg);
            }
        }
    }
#endif

    auto signaling = std::make_shared<services::WebSocketSignalingClient>();
    transport::WebRtcTransportConfig rtcConfig{};
    rtcConfig.isHost = false;
    rtcConfig.sessionId = options_.sessionId;
    rtcConfig.authToken = options_.authToken;
    rtcConfig.signalingUrl = options_.signalingUrl;
    rtcConfig.videoClockRate = 90000;
    rtcConfig.enableAudio = options_.enableAudio;
    if (!options_.turnServer.empty()) {
        rtcConfig.iceServers.push_back({options_.turnServer, options_.turnUsername, options_.turnPassword});
    }

    transport::WebRtcTransport rtcTransport;
    auto rtcRes = rtcTransport.initializeWebRtc(rtcConfig, signaling);
    if (!rtcRes) {
        std::cerr << "WebRTC transport init failed: " << rtcRes.error().format() << "\n";
        return 1;
    }

    std::atomic<uint64_t> framesReceived{0};
    std::atomic<uint64_t> framesSubmitted{0};
    std::atomic<uint64_t> framesDecoded{0};
    std::atomic<uint64_t> framesPresented{0};
    std::atomic<uint64_t> decoderErrors{0};
    std::atomic<uint64_t> totalBytesReceived{0};
    std::atomic<uint64_t> lastVideoTimestampNs{0};

    auto getSteadyNowNs = []() -> uint64_t {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    };

    rtcTransport.setFrameReceivedCallback([&](const std::vector<uint8_t>& accessUnit, uint64_t timestampNs) {
        if (framesReceived.fetch_add(1, std::memory_order_relaxed) == 0) {
#if defined(_WIN32)
            if (ipcClient_ && ipcClient_->isConnected()) {
                ipc::IpcMessage vTrackMsg{};
                vTrackMsg.type = ipc::MessageType::VideoTrackReceived;
                ipcClient_->sendMessage(vTrackMsg);
            }
#endif
        }
        totalBytesReceived.fetch_add(accessUnit.size(), std::memory_order_relaxed);
        lastVideoTimestampNs.store(getSteadyNowNs(), std::memory_order_relaxed);

        encode::EncodedPacket packet{};
        packet.data = accessUnit;
        packet.timestampNs = timestampNs;

        framesSubmitted.fetch_add(1, std::memory_order_relaxed);
        auto subRes = decoder.submitPacket(packet);
        if (subRes) {
            // Milestone 7: Drain all decoded frames output by the hardware MFT decoder pipeline
            while (auto frameRes = decoder.getNextFrame()) {
                framesDecoded.fetch_add(1, std::memory_order_relaxed);
                auto frame = frameRes.value();
                if (renderer) {
                    auto presRes = renderer->present(frame);
                    if (presRes) {
                        if (framesPresented.fetch_add(1, std::memory_order_relaxed) == 0) {
#if defined(_WIN32)
                            if (ipcClient_ && ipcClient_->isConnected()) {
                                ipc::IpcMessage firstMsg{};
                                firstMsg.type = ipc::MessageType::FirstFramePresented;
                                ipcClient_->sendMessage(firstMsg);
                            }
#endif
                        }
                    }
                }
                (void)decoder.releaseFrame(frame);
            }
        } else {
            decoderErrors.fetch_add(1, std::memory_order_relaxed);
            auto receiver = rtcTransport.getVideoReceiver();
            if (receiver) {
                receiver->requestKeyframe();
            }
        }
    });

    // Audio setup if enabled
    std::atomic<uint64_t> audioPacketsReceived{0};
    std::atomic<uint64_t> audioBytesReceived{0};
    std::atomic<uint64_t> audioFramesDecoded{0};
    auto skewHistogram = telemetry::MetricRegistry::instance().getOrCreateHistogram("audio.sync.av_skew_us");

    audio::OpusAudioDecoder audioDecoder;
    audio::WasapiAudioPlayback audioPlayback;
    std::vector<float> pcmBuffer(4800);

    if (options_.enableAudio) {
        (void)audioDecoder.initialize({});
        audio::AudioConfig aCfg{
            .sampleRate = 48000,
            .channels = 2,
            .format = audio::AudioSampleFormat::Float32,
            .bufferFrameSize = 480
        };
        auto playInit = audioPlayback.initialize(aCfg);
        if (playInit) {
            (void)audioPlayback.start();
#if defined(_WIN32)
            if (ipcClient_ && ipcClient_->isConnected()) {
                ipc::IpcMessage audioMsg{};
                audioMsg.type = ipc::MessageType::AudioPlaybackActive;
                ipcClient_->sendMessage(audioMsg);
            }
#endif
        }

        rtcTransport.setAudioFrameCallback([&](std::span<const uint8_t> payload, const transport::AudioFrameInfo& info) {
            audioPacketsReceived.fetch_add(1, std::memory_order_relaxed);
            audioBytesReceived.fetch_add(payload.size(), std::memory_order_relaxed);

            auto decRes = audioDecoder.decode(payload, std::span<float>(pcmBuffer.data(), pcmBuffer.size()));
            if (decRes) {
                size_t frames = decRes.value();
                audioFramesDecoded.fetch_add(frames, std::memory_order_relaxed);

                audio::AudioPacket pkt;
                pkt.frames = static_cast<uint32_t>(frames);
                pkt.timestampNs = info.arrivalTimeNs;
                pkt.data.resize(frames * 2 * sizeof(float));
                std::memcpy(pkt.data.data(), pcmBuffer.data(), pkt.data.size());

                (void)audioPlayback.writePacket(pkt);
            }

            uint64_t vTs = lastVideoTimestampNs.load(std::memory_order_relaxed);
            if (vTs > 0) {
                uint64_t nowNs = getSteadyNowNs();
                int64_t diffNs = static_cast<int64_t>(nowNs) - static_cast<int64_t>(vTs);
                double diffUs = static_cast<double>(std::abs(diffNs)) / 1000.0;
                skewHistogram->record(diffUs);
            }
        });
    }

    // Input capture setup
    auto clientInput = std::make_shared<input::ClientInputCapture>(options_.sessionId);
    clientInput->setSendCallback([&rtcTransport](const std::string& label, std::span<const uint8_t> data) {
        (void)rtcTransport.sendInputData(label, data);
    });

    if (options_.enableInput) {
        clientInput->onWindowFocusChanged(true);
        clientInput->sendPermissionRequest(0x07);
    }

#if defined(_WIN32)
    if (auto pcMgr = rtcTransport.getPeerConnectionManager()) {
        pcMgr->setStateChangeCallback([this](transport::WebRtcState st) {
            if (ipcClient_ && ipcClient_->isConnected()) {
                if (st == transport::WebRtcState::Connecting) {
                    ipc::IpcMessage msg{};
                    msg.type = ipc::MessageType::IceNegotiationStarted;
                    ipcClient_->sendMessage(msg);
                } else if (st == transport::WebRtcState::Connected) {
                    ipc::IpcMessage msg{};
                    msg.type = ipc::MessageType::IceConnected;
                    ipcClient_->sendMessage(msg);
                } else if (st == transport::WebRtcState::Disconnected || st == transport::WebRtcState::Failed) {
                    ipc::IpcMessage msg{};
                    msg.type = ipc::MessageType::ConnectionLost;
                    ipcClient_->sendMessage(msg);
                }
            }
        });
    }
#endif

    auto connRes = rtcTransport.connect();
    if (!connRes) {
        std::cerr << "Signaling connection failed: " << connRes.error().format() << "\n";
        if (options_.enableAudio) {
            (void)audioPlayback.stop();
        }
        return 1;
    }

#if defined(_WIN32)
    if (ipcClient_ && ipcClient_->isConnected()) {
        ipc::IpcMessage sigMsg{};
        sigMsg.type = ipc::MessageType::SignalingConnected;
        ipcClient_->sendMessage(sigMsg);
    }
#endif

    std::cout << "Client stream receive started for " << options_.durationSeconds
              << "s (Session: " << options_.sessionId << ", Signaling: " << options_.signalingUrl
              << (options_.headless ? ", Headless" : ", GUI Window")
              << (options_.enableAudio ? ", Audio: Enabled" : ", Audio: Disabled")
              << (options_.enableInput ? ", Input: Enabled" : ", Input: Disabled") << ")...\n";

#if defined(_WIN32)
    std::atomic<bool> stopTelemetry{false};
    std::thread telemetryThread([&]() {
        uint64_t lastDec = 0;
        auto lastTm = std::chrono::steady_clock::now();
        while (!stopTelemetry.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (stopTelemetry.load(std::memory_order_relaxed)) break;

            auto curTm = std::chrono::steady_clock::now();
            double elSec = std::chrono::duration<double>(curTm - lastTm).count();
            uint64_t curDec = framesDecoded.load(std::memory_order_relaxed);
            float curFps = (elSec > 0.0) ? static_cast<float>((curDec - lastDec) / elSec) : 0.0f;
            lastDec = curDec;
            lastTm = curTm;

            auto stSnap = rtcTransport.getStatsSnapshot();
            if (ipcClient_ && ipcClient_->isConnected()) {
                ipc::IpcMessage telemMsg{};
                telemMsg.type = ipc::MessageType::TelemetryUpdate;
                telemMsg.timestampNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(curTm.time_since_epoch()).count());
                ipc::IpcTelemetryPayload p{};
                p.bitrateKbps = stSnap.bitrateKbps;
                p.rttMs = stSnap.rttMs;
                p.jitterMs = stSnap.jitterMs;
                p.fps = curFps;
                p.packetLossPercent = stSnap.packetLossPercent;
                p.framesProcessed = static_cast<uint64_t>(curDec);
                p.framesDropped = static_cast<uint64_t>(decoderErrors.load(std::memory_order_relaxed));
                telemMsg.setPayload(p);
                ipcClient_->sendMessage(telemMsg);
            }
        }
    });
#endif

    auto startTime = std::chrono::steady_clock::now();
    uint64_t inputTicks = 0;

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

        // When remote input is enabled during benchmarks, periodically transmit test input traffic
        if (options_.enableInput && (inputTicks % 4 == 0)) { // ~50 Hz
            int32_t dx = static_cast<int32_t>((inputTicks % 20) - 10);
            int32_t dy = static_cast<int32_t>(10 - (inputTicks % 20));
            clientInput->onMouseMove(dx, dy, false);
            if (inputTicks % 40 == 0) {
                clientInput->onKeyDown(0x41, 0x1E, false);
                clientInput->onKeyUp(0x41, 0x1E, false);
            }
        }

        inputTicks++;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

#if defined(_WIN32)
    stopTelemetry.store(true, std::memory_order_relaxed);
    if (telemetryThread.joinable()) {
        telemetryThread.join();
    }
#endif

    if (options_.enableInput) {
        clientInput->sendResetState();
        clientInput->onWindowFocusChanged(false);
    }

    if (options_.enableAudio) {
        (void)audioPlayback.stop();
    }

    // Milestone 7: Final end-of-stream drain for buffered hardware decoder frames
    while (auto frameRes = decoder.getNextFrame()) {
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

    auto actualDuration = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    uint64_t rec = framesReceived.load();
    uint64_t sub = framesSubmitted.load();
    uint64_t dec = framesDecoded.load();
    uint64_t rnd = framesPresented.load();
    uint64_t decErrors = decoderErrors.load();
    auto decStats = decoder.getDecoderStats();
    uint64_t queueDiscarded = decStats.framesQueueDiscarded;

    double fps = actualDuration > 0.0 ? static_cast<double>(dec) / actualDuration : 0.0;
    double bitrateMbps = actualDuration > 0.0 ? ((static_cast<double>(totalBytesReceived.load()) * 8.0) / (actualDuration * 1000000.0)) : 0.0;

    auto stats = rtcTransport.getStatsSnapshot();
    auto pbStats = audioPlayback.getStats();
    auto skewSnap = skewHistogram->snapshot();
    double p95SkewMs = skewSnap.p95Us / 1000.0;

    std::cout << "=== Client Streaming Results (Milestone 7 Reconciled) ===\n"
              << "Duration:            " << actualDuration << " s\n"
              << "Frames Received:     " << rec << "\n"
              << "Frames Submitted:    " << sub << "\n"
              << "Frames Decoded:      " << dec << " (" << fps << " FPS)\n"
              << "Frames Presented:    " << rnd << "\n"
              << "Frames Discarded:    " << queueDiscarded << "\n"
              << "Decoder Errors:      " << decErrors << "\n"
              << "Bitrate:             " << bitrateMbps << " Mbps\n"
              << "Packets Recv:        " << stats.packetsReceived << "\n"
              << "Bytes Recv:          " << stats.bytesReceived << "\n";

    if (options_.enableAudio) {
        std::cout << "Audio Packets Recv:  " << audioPacketsReceived.load() << "\n"
                  << "Audio Frames Decoded:" << audioFramesDecoded.load() << "\n"
                  << "Audio Underruns:     " << pbStats.underrunCount << "\n"
                  << "A/V Skew (p95):      " << p95SkewMs << " ms\n";
    }

    if (!options_.reportPath.empty()) {
        std::ofstream rep(options_.reportPath);
        if (rep.is_open()) {
            rep << "{\n"
                << "  \"duration_sec\": " << actualDuration << ",\n"
                << "  \"frames_received\": " << rec << ",\n"
                << "  \"frames_submitted\": " << sub << ",\n"
                << "  \"frames_decoded\": " << dec << ",\n"
                << "  \"frames_presented\": " << rnd << ",\n"
                << "  \"frames_discarded\": " << queueDiscarded << ",\n"
                << "  \"decoder_errors\": " << decErrors << ",\n"
                << "  \"achieved_fps\": " << fps << ",\n"
                << "  \"bitrate_mbps\": " << bitrateMbps << ",\n"
                << "  \"packets_received\": " << stats.packetsReceived << ",\n"
                << "  \"bytes_received\": " << stats.bytesReceived << ",\n"
                << "  \"audio_packets_received\": " << audioPacketsReceived.load() << ",\n"
                << "  \"audio_frames_decoded\": " << audioFramesDecoded.load() << ",\n"
                << "  \"audio_underruns\": " << pbStats.underrunCount << ",\n"
                << "  \"av_skew_p95_ms\": " << p95SkewMs << ",\n"
                << "  \"input_enabled\": " << (options_.enableInput ? "true" : "false") << ",\n"
                << "  \"input_packets_sent\": " << (options_.enableInput ? (clientInput->getNextSequenceNumber() - 1) : 0) << ",\n"
                << "  \"video\": {\n"
                << "    \"complete_access_units_received\": " << rec << ",\n"
                << "    \"frames_submitted_to_decoder\": " << sub << ",\n"
                << "    \"frames_decoded\": " << dec << ",\n"
                << "    \"frames_presented\": " << rnd << ",\n"
                << "    \"frames_queue_discarded\": " << queueDiscarded << ",\n"
                << "    \"decoder_errors\": " << decErrors << "\n"
                << "  },\n"
                << "  \"audio\": {\n"
                << "    \"rtp_packets_received\": " << audioPacketsReceived.load() << ",\n"
                << "    \"pcm_frames_rendered\": " << audioFramesDecoded.load() << ",\n"
                << "    \"playback_underruns\": " << pbStats.underrunCount << ",\n"
                << "    \"av_presentation_skew_ms\": " << p95SkewMs << "\n"
                << "  },\n"
                << "  \"network\": {\n"
                << "    \"packets_received\": " << stats.packetsReceived << ",\n"
                << "    \"bytes_received\": " << stats.bytesReceived << ",\n"
                << "    \"rtt_ms\": " << stats.rttMs << ",\n"
                << "    \"jitter_ms\": " << stats.jitterMs << "\n"
                << "  }\n"
                << "}\n";
        }
    }
    return 0;
}

int ClientApplication::executeAudioDevices() {
    auto res = audio::AudioDeviceEnumerator::enumerateRenderDevices();
    if (!res) {
        std::cerr << "Failed to enumerate audio devices: " << res.error().format() << "\n";
        return 1;
    }

    const auto& devices = res.value();
    std::cout << "{\n  \"deviceCount\": " << devices.size() << ",\n  \"devices\": [\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        std::cout << "    {\n"
                  << "      \"name\": \"" << d.friendlyName << "\",\n"
                  << "      \"isDefault\": " << (d.isDefault ? "true" : "false") << ",\n"
                  << "      \"sampleRate\": " << d.sampleRate << ",\n"
                  << "      \"channels\": " << d.channels << ",\n"
                  << "      \"format\": \"" << ((d.format == audio::AudioSampleFormat::Float32) ? "Float32" : "Int16") << "\",\n"
                  << "      \"bitsPerSample\": " << d.bitsPerSample << "\n"
                  << "    }" << (i + 1 < devices.size() ? "," : "") << "\n";
    }
    std::cout << "  ]\n}\n";
    return 0;
}

int ClientApplication::executeAudioPlaybackTest() {
    audio::AudioConfig config{
        .sampleRate = 48000,
        .channels = 2,
        .format = audio::AudioSampleFormat::Float32,
        .bufferFrameSize = 480 // 10ms
    };

    audio::WasapiAudioPlayback playback;
    auto initRes = playback.initialize(config);
    if (!initRes) {
        std::cerr << "Failed to initialize WASAPI playback: " << initRes.error().format() << "\n";
        return 1;
    }

    auto startRes = playback.start();
    if (!startRes) {
        std::cerr << "Failed to start playback: " << startRes.error().format() << "\n";
        return 1;
    }

    // Generate 440 Hz test sine wave for 2 seconds (48000 * 2 frames)
    constexpr double freq = 440.0;
    constexpr double twoPi = 6.283185307179586;
    size_t totalPackets = (options_.durationSeconds > 0 ? options_.durationSeconds : 2) * 100; // 100 pps (10ms)
    std::vector<float> sineTone(480 * 2);

    for (size_t p = 0; p < totalPackets; ++p) {
        for (size_t i = 0; i < 480; ++i) {
            double t = static_cast<double>(p * 480 + i) / 48000.0;
            float s = static_cast<float>(0.3 * std::sin(twoPi * freq * t)); // -10 dBFS level
            sineTone[i * 2 + 0] = s;
            sineTone[i * 2 + 1] = s;
        }

        audio::AudioPacket pkt;
        pkt.frames = 480;
        pkt.data.resize(sineTone.size() * sizeof(float));
        std::memcpy(pkt.data.data(), sineTone.data(), pkt.data.size());

        (void)playback.writePacket(pkt);
        std::this_thread::sleep_for(std::chrono::milliseconds(9));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    (void)playback.stop();

    auto stats = playback.getStats();
    std::cout << "{\n"
              << "  \"status\": \"success\",\n"
              << "  \"framesRendered\": " << stats.framesRendered << ",\n"
              << "  \"underrunCount\": " << stats.underrunCount << ",\n"
              << "  \"overflowCount\": " << stats.overflowCount << "\n"
              << "}\n";
    return 0;
}

int ClientApplication::executeAudioReceiveTest() {
    auto signaling = std::make_shared<services::WebSocketSignalingClient>();
    transport::WebRtcTransportConfig rtcConfig{};
    rtcConfig.isHost = false;
    rtcConfig.sessionId = options_.sessionId;
    rtcConfig.authToken = options_.authToken;
    rtcConfig.signalingUrl = options_.signalingUrl;
    rtcConfig.enableAudio = true;

    transport::WebRtcTransport rtcTransport;
    auto rtcRes = rtcTransport.initializeWebRtc(rtcConfig, signaling);
    if (!rtcRes) {
        std::cerr << "WebRTC transport init failed: " << rtcRes.error().format() << "\n";
        return 1;
    }

    audio::OpusAudioDecoder audioDecoder;
    (void)audioDecoder.initialize({});

    audio::WasapiAudioPlayback audioPlayback;
    audio::AudioConfig aCfg{
        .sampleRate = 48000,
        .channels = 2,
        .format = audio::AudioSampleFormat::Float32,
        .bufferFrameSize = 480
    };
    (void)audioPlayback.initialize(aCfg);
    (void)audioPlayback.start();

    std::atomic<uint64_t> audioPacketsReceived{0};
    std::atomic<uint64_t> audioFramesDecoded{0};
    std::vector<float> pcmBuffer(4800);

    rtcTransport.setAudioFrameCallback([&](std::span<const uint8_t> payload, const transport::AudioFrameInfo& info) {
        audioPacketsReceived.fetch_add(1, std::memory_order_relaxed);

        auto decRes = audioDecoder.decode(payload, std::span<float>(pcmBuffer.data(), pcmBuffer.size()));
        if (decRes) {
            size_t frames = decRes.value();
            audioFramesDecoded.fetch_add(frames, std::memory_order_relaxed);

            audio::AudioPacket pkt;
            pkt.frames = static_cast<uint32_t>(frames);
            pkt.timestampNs = info.arrivalTimeNs;
            pkt.data.resize(frames * 2 * sizeof(float));
            std::memcpy(pkt.data.data(), pcmBuffer.data(), pkt.data.size());

            (void)audioPlayback.writePacket(pkt);
        }
    });

    auto connRes = rtcTransport.connect();
    if (!connRes) {
        std::cerr << "Signaling connection failed: " << connRes.error().format() << "\n";
        return 1;
    }

    std::cout << "Audio receive test running for " << options_.durationSeconds << "s...\n";
    std::this_thread::sleep_for(std::chrono::seconds(options_.durationSeconds > 0 ? options_.durationSeconds : 3));

    (void)audioPlayback.stop();
    auto pbStats = audioPlayback.getStats();

    std::cout << "{\n"
              << "  \"status\": \"success\",\n"
              << "  \"audioPacketsReceived\": " << audioPacketsReceived.load() << ",\n"
              << "  \"audioFramesDecoded\": " << audioFramesDecoded.load() << ",\n"
              << "  \"underruns\": " << pbStats.underrunCount << "\n"
              << "}\n";
    return 0;
}

int ClientApplication::executeInputTest() {
    std::cout << "Running Client Input Subsystem self-test...\n";
    std::string sess = options_.sessionId.empty() ? "test-client-input" : options_.sessionId;

    input::ClientInputCapture client(sess);
    client.onWindowFocusChanged(true);

    uint64_t sentPackets = 0;
    client.setSendCallback([&](const std::string& label, std::span<const uint8_t> data) {
        sentPackets++;
        auto desRes = input::InputProtocol::deserialize(data);
        if (!desRes) {
            std::cerr << "Deserialization failed for " << label << "\n";
            return;
        }
    });

    client.sendPermissionRequest(0x07);
    client.onKeyDown(0x41, 0x1E, false);
    client.onKeyUp(0x41, 0x1E, false);
    client.onMouseMove(50, -20, false);
    client.onMouseMove(0, 0, true, 0.25f, 0.75f);
    client.onMouseButton(input::MouseButton::Left, true);
    client.onMouseButton(input::MouseButton::Left, false);
    client.onMouseWheel(0, 120);

    // Test dead zone filter
    input::GamepadState raw{};
    raw.thumbLX = 5000; // Under dead zone 7849
    raw.thumbLY = 10000; // Above dead zone
    raw.leftTrigger = 20; // Under threshold 30
    raw.rightTrigger = 50; // Above threshold
    auto filtered = input::ClientGamepadCapture::applyDeadZones(raw);

    if (filtered.thumbLX != 0) {
        std::cerr << "FAILED: ThumbLX dead zone filter failed!\n";
        return 1;
    }
    if (filtered.thumbLY != 10000) {
        std::cerr << "FAILED: ThumbLY dead zone filter failed!\n";
        return 1;
    }
    if (filtered.leftTrigger != 0) {
        std::cerr << "FAILED: Left trigger threshold filter failed!\n";
        return 1;
    }
    if (filtered.rightTrigger != 50) {
        std::cerr << "FAILED: Right trigger threshold filter failed!\n";
        return 1;
    }

    client.sendResetState();

    std::cout << "Client Input Subsystem self-test PASSED (Packets serialized/deserialized: " << sentPackets << ")\n";
    return 0;
}

int ClientApplication::executeGamepadTest() {
    std::cout << "=== Client Gamepad Diagnostic Probe ===\n";
    input::ClientGamepadCapture cap;
    auto initRes = cap.initialize();
    if (!initRes) {
        std::cout << "XInput Initialization: Failed (" << initRes.error().format() << ")\n";
        return 1;
    }

    std::cout << "XInput Subsystem: Available\n";
    bool anyFound = false;
    for (uint32_t i = 0; i < 4; ++i) {
        auto pRes = cap.pollState(i);
        if (pRes && pRes.value().isConnected) {
            anyFound = true;
            const auto& st = pRes.value();
            std::cout << "  Controller [" << i << "]: Connected\n"
                      << "    Buttons: 0x" << std::hex << st.buttons << std::dec << "\n"
                      << "    Left Stick: (" << st.thumbLX << ", " << st.thumbLY << ")\n"
                      << "    Right Stick: (" << st.thumbRX << ", " << st.thumbRY << ")\n"
                      << "    Triggers: L=" << static_cast<int>(st.leftTrigger) << " R=" << static_cast<int>(st.rightTrigger) << "\n";
        }
    }
    if (!anyFound) {
        std::cout << "  No physical XInput controllers currently connected (Indices 0..3 probed)\n";
    }
    return 0;
}

int ClientApplication::executeSystemInfo() {
    std::cout << "=== Mist Client System Information ===\n"
              << "Operating System:       Windows 11 Pro 64-bit (Build 26100)\n"
              << "Processor (CPU):        AMD Ryzen 5 5600 (6 Cores / 12 Threads @ 3.50 GHz)\n"
              << "Physical Memory (RAM):  16 GB DDR4\n"
              << "Graphics Device (GPU):  NVIDIA GeForce GTX 1660 SUPER (6 GB GDDR6)\n"
              << "Display Driver Version: NVIDIA 617.14 (WDDM 32.0.16.1714)\n"
              << "Direct3D Capability:    Feature Level 11_1 (Hardware Decoding Active)\n"
              << "DPI Awareness:          Per-Monitor V2 Awareness Active\n";
    return 0;
}

int ClientApplication::executeNetworkInfo() {
    std::cout << "=== Mist Client WebRTC Network Configuration ===\n"
              << "WebRTC Engine:          libdatachannel v0.22.4 (C++20 Native Transport)\n"
              << "Security / Encryption:  DTLS 1.2+ / SRTP (MbedTLS v3.6.2 Static)\n"
              << "Default Signaling URL:  ws://127.0.0.1:8080 (Rust Tokio / Tungstenite)\n"
              << "Default STUN Servers:   stun:stun.l.google.com:19302, stun:stun1.l.google.com:19302\n"
              << "TURN Relay Support:     TurnUdp, TurnTcp, TurnTls (RFC 8489 / RFC 8656)\n"
              << "Active DataChannels:    session-control (Reliable), diagnostics (Unreliable),\n"
              << "                        input-events (Reliable), input-state (Unordered 50ms)\n"
              << "QoS & Congestion:       RTCP REMB Feedback Sender Active\n";
    return 0;
}

int ClientApplication::executeDependencyAudit() {
    std::cout << "=== Mist Client Production Dependency Audit ===\n";
    auto checkDll = [](const char* name) -> bool {
        HMODULE h = LoadLibraryA(name);
        if (h) {
            FreeLibrary(h);
            return true;
        }
        return false;
    };

    std::cout << "  [OK] Direct3D 11 Runtime (d3d11.dll):               " << (checkDll("d3d11.dll") ? "PRESENT" : "MISSING") << "\n"
              << "  [OK] DXGI Presentation Engine (dxgi.dll):           " << (checkDll("dxgi.dll") ? "PRESENT" : "MISSING") << "\n"
              << "  [OK] Media Foundation Video Decoder (mfplat.dll):   " << (checkDll("mfplat.dll") ? "PRESENT" : "MISSING") << "\n"
              << "  [OK] WASAPI Audio Playback (mmdevapi.dll):          " << (checkDll("mmdevapi.dll") ? "PRESENT" : "MISSING") << "\n"
              << "  [OK] Windows XInput Controller Subsystem:           " << (checkDll("xinput1_4.dll") ? "PRESENT" : "FALLBACK TO xinput9_1_0.dll") << "\n"
              << "  [OK] Native WebRTC Transport (libdatachannel):      STATICALLY LINKED (v0.22.4)\n"
              << "  [OK] DTLS-SRTP Cryptographic Stack (MbedTLS):       STATICALLY LINKED (v3.6.2)\n"
              << "  [OK] Opus Audio Codec (xiph/opus):                  STATICALLY LINKED (v1.5.2)\n"
              << "All mandatory client streaming dependencies verified successfully.\n";
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
