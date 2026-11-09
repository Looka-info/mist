#include <gtest/gtest.h>
#include "mist/decode/CrossPlatformDecoders.h"
#include "mist/decode/GuestClientSession.h"
#include "mist/input/InputProtocol.h"
#include <vector>
#include <string>

using namespace mist;
using namespace mist::decode;
using namespace mist::encode;
using namespace mist::input;

TEST(CrossPlatformDecoderFactoryTest, CreateDecodersForAllPlatforms) {
    // Platform string representations
    EXPECT_EQ(clientPlatformToString(ClientPlatform::Windows_D3D11VA), "Windows (D3D11VA)");
    EXPECT_EQ(clientPlatformToString(ClientPlatform::MacOS_VideoToolbox), "macOS (Apple VideoToolbox)");
    EXPECT_EQ(clientPlatformToString(ClientPlatform::Linux_VaApi), "Linux (VA-API / DMA-BUF)");
    EXPECT_EQ(clientPlatformToString(ClientPlatform::Android_MediaCodec), "Android (NDK AMediaCodec)");
    EXPECT_EQ(clientPlatformToString(ClientPlatform::Web_WebRtcWasm), "Web (WebCodecs WASM)");

#if defined(_WIN32)
    EXPECT_EQ(VideoDecoderFactory::getNativePlatform(), ClientPlatform::Windows_D3D11VA);
    auto winDec = VideoDecoderFactory::createDecoder(ClientPlatform::Windows_D3D11VA);
    EXPECT_TRUE(winDec.hasValue());
    EXPECT_NE(winDec.value(), nullptr);
#endif

    // Cross-platform decoders should all instantiate successfully
    auto macDec = VideoDecoderFactory::createDecoder(ClientPlatform::MacOS_VideoToolbox);
    ASSERT_TRUE(macDec.hasValue());
    EXPECT_NE(macDec.value(), nullptr);

    auto linuxDec = VideoDecoderFactory::createDecoder(ClientPlatform::Linux_VaApi);
    ASSERT_TRUE(linuxDec.hasValue());
    EXPECT_NE(linuxDec.value(), nullptr);

    auto androidDec = VideoDecoderFactory::createDecoder(ClientPlatform::Android_MediaCodec);
    ASSERT_TRUE(androidDec.hasValue());
    EXPECT_NE(androidDec.value(), nullptr);

    auto webDec = VideoDecoderFactory::createDecoder(ClientPlatform::Web_WebRtcWasm);
    ASSERT_TRUE(webDec.hasValue());
    EXPECT_NE(webDec.value(), nullptr);
}

TEST(MacOSVideoToolboxTest, DecodeLifecycle) {
    auto decRes = VideoToolboxDecoder::create();
    ASSERT_TRUE(decRes.hasValue());
    auto decoder = std::move(decRes.value());

    EXPECT_FALSE(decoder->isInitialized());

    DecodeConfig cfg{
        .codec = VideoCodec::H264,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    if (VideoToolboxDecoder::isNativePlatform()) {
        auto initRes = decoder->initialize(cfg);
        EXPECT_TRUE(initRes.hasValue());
        EXPECT_TRUE(decoder->isInitialized());

        EncodedPacket emptyPkt{};
        EXPECT_FALSE(decoder->submitPacket(emptyPkt).hasValue());

        EXPECT_TRUE(decoder->flush().hasValue());
        EXPECT_TRUE(decoder->shutdown().hasValue());
        EXPECT_FALSE(decoder->isInitialized());
    } else {
        auto initRes = decoder->initialize(cfg);
        EXPECT_FALSE(initRes.hasValue());
        EXPECT_EQ(initRes.error().code(), common::ErrorCode::NotImplemented);
        EXPECT_FALSE(decoder->isInitialized());
        EXPECT_TRUE(decoder->shutdown().hasValue());
    }
}

TEST(LinuxVaApiTest, SurfaceRingBufferLifecycle) {
    auto decRes = VaApiVideoDecoder::create();
    ASSERT_TRUE(decRes.hasValue());
    auto decoder = std::move(decRes.value());

    DecodeConfig cfg{
        .codec = VideoCodec::HEVC,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    if (VaApiVideoDecoder::isNativePlatform()) {
        auto initRes = decoder->initialize(cfg);
        if (initRes.hasValue()) {
            EXPECT_TRUE(decoder->isInitialized());
            EncodedPacket pkt{};
            pkt.data = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01};
            pkt.timestampNs = 16666666ULL;
            (void)decoder->submitPacket(pkt);
            EXPECT_TRUE(decoder->flush().hasValue());
            EXPECT_TRUE(decoder->shutdown().hasValue());
        }
    } else {
        auto initRes = decoder->initialize(cfg);
        EXPECT_FALSE(initRes.hasValue());
        EXPECT_EQ(initRes.error().code(), common::ErrorCode::NotImplemented);
        EXPECT_FALSE(decoder->isInitialized());
        EXPECT_TRUE(decoder->shutdown().hasValue());
    }
}

TEST(AndroidMediaCodecTest, BufferQueueAndDecode) {
    auto decRes = MediaCodecVideoDecoder::create();
    ASSERT_TRUE(decRes.hasValue());
    auto decoder = std::move(decRes.value());

    DecodeConfig cfg{
        .codec = VideoCodec::H264,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    if (MediaCodecVideoDecoder::isNativePlatform()) {
        auto initRes = decoder->initialize(cfg);
        if (initRes.hasValue()) {
            EXPECT_TRUE(decoder->isInitialized());
            EncodedPacket emptyPkt{};
            EXPECT_FALSE(decoder->submitPacket(emptyPkt).hasValue());
            EXPECT_TRUE(decoder->flush().hasValue());
            EXPECT_TRUE(decoder->shutdown().hasValue());
        }
    } else {
        auto initRes = decoder->initialize(cfg);
        EXPECT_FALSE(initRes.hasValue());
        EXPECT_EQ(initRes.error().code(), common::ErrorCode::NotImplemented);
        EXPECT_FALSE(decoder->isInitialized());
        EXPECT_TRUE(decoder->shutdown().hasValue());
    }
}

TEST(WebRtcWasmTest, WebCodecsPipeline) {
    auto decRes = WebRtcWasmVideoDecoder::create();
    ASSERT_TRUE(decRes.hasValue());
    auto decoder = std::move(decRes.value());

    DecodeConfig cfg{
        .codec = VideoCodec::AV1,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    if (WebRtcWasmVideoDecoder::isNativePlatform()) {
        auto initRes = decoder->initialize(cfg);
        if (initRes.hasValue()) {
            EXPECT_TRUE(decoder->isInitialized());
            EncodedPacket emptyPkt{};
            EXPECT_FALSE(decoder->submitPacket(emptyPkt).hasValue());
            EXPECT_TRUE(decoder->flush().hasValue());
            EXPECT_TRUE(decoder->shutdown().hasValue());
        }
    } else {
        auto initRes = decoder->initialize(cfg);
        EXPECT_FALSE(initRes.hasValue());
        EXPECT_EQ(initRes.error().code(), common::ErrorCode::NotImplemented);
        EXPECT_FALSE(decoder->isInitialized());
        EXPECT_TRUE(decoder->shutdown().hasValue());
    }
}


TEST(GuestClientSessionTest, LifecycleAndVideoIngestion) {
    GuestClientConfig config{
        .platform = VideoDecoderFactory::getNativePlatform(),
        .sessionId = "session-test-42",
        .preferredCodec = VideoCodec::H264,
        .enableAudio = true,
        .enableInput = true
    };

    auto sessionRes = GuestClientSession::create(config);
    ASSERT_TRUE(sessionRes.hasValue());
    auto session = std::move(sessionRes.value());

    EXPECT_FALSE(session->isConnected());
    EXPECT_EQ(session->getPlatform(), VideoDecoderFactory::getNativePlatform());

    bool connectionStateObserved = false;
    session->setConnectionStateCallback([&](bool connected) {
        connectionStateObserved = connected;
    });

    uint32_t framesReceivedCount = 0;
    session->setFrameDecodedCallback([&](const DecodedFrame& frame) {
        ++framesReceivedCount;
        EXPECT_GT(frame.width, 0u);
        EXPECT_GT(frame.height, 0u);
    });

    EXPECT_TRUE(session->connect().hasValue());
    EXPECT_TRUE(session->isConnected());
    EXPECT_TRUE(connectionStateObserved);

    // Ingest audio packet
    std::vector<uint8_t> opusPacket = {0xF8, 0xFF, 0xFE};
    EXPECT_TRUE(session->ingestAudioPacket(opusPacket.data(), opusPacket.size(), 1000ULL).hasValue());

    auto stats = session->getStats();
    EXPECT_EQ(stats.audioPacketsReceived, 1u);
    EXPECT_EQ(stats.activeCodec, "H.264");
    EXPECT_EQ(stats.platformName, clientPlatformToString(VideoDecoderFactory::getNativePlatform()));

    EXPECT_TRUE(session->disconnect().hasValue());
    EXPECT_FALSE(session->isConnected());
    EXPECT_FALSE(connectionStateObserved);
}


TEST(GuestClientSessionTest, InputPackagingAndDispatch) {
    GuestClientConfig config{
        .platform = VideoDecoderFactory::getNativePlatform(),
        .sessionId = "session-input-101",
        .enableInput = true
    };

    auto sessionRes = GuestClientSession::create(config);
    ASSERT_TRUE(sessionRes.hasValue());
    auto session = std::move(sessionRes.value());

    EXPECT_TRUE(session->connect().hasValue());

    std::vector<std::vector<uint8_t>> dispatchedPackets;
    session->setInputOutboundCallback([&](const std::vector<uint8_t>& bytes) {
        dispatchedPackets.push_back(bytes);
    });

    // 1. Mouse move
    EXPECT_TRUE(session->sendMouseMove(25, -10, false, 0.0f, 0.0f).hasValue());

    // 2. Mouse button
    EXPECT_TRUE(session->sendMouseButton(MouseButton::Right, true).hasValue());

    // 3. Keyboard
    EXPECT_TRUE(session->sendKeyboard(0x42, KeyAction::Press, 0x30).hasValue());

    // 4. Gamepad
    GamepadState g{};
    g.index = 1;
    g.buttons = 0x1000;
    g.leftTrigger = 200;
    EXPECT_TRUE(session->sendGamepad(g).hasValue());

    // 5. Clipboard
    EXPECT_TRUE(session->sendClipboard("Cross-platform sync text").hasValue());

    ASSERT_EQ(dispatchedPackets.size(), 5u);

    // Verify 1: Mouse move deserialization
    auto parse1 = InputProtocol::deserialize(dispatchedPackets[0]);
    ASSERT_TRUE(parse1.hasValue());
    EXPECT_TRUE(std::holds_alternative<MouseMoveEvent>(parse1.value().second));
    const auto& mm = std::get<MouseMoveEvent>(parse1.value().second);
    EXPECT_EQ(mm.deltaX, 25);
    EXPECT_EQ(mm.deltaY, -10);

    // Verify 2: Mouse button deserialization
    auto parse2 = InputProtocol::deserialize(dispatchedPackets[1]);
    ASSERT_TRUE(parse2.hasValue());
    EXPECT_TRUE(std::holds_alternative<MouseButtonEvent>(parse2.value().second));
    const auto& mb = std::get<MouseButtonEvent>(parse2.value().second);
    EXPECT_EQ(mb.button, MouseButton::Right);
    EXPECT_TRUE(mb.isDown);

    // Verify 3: Key event deserialization
    auto parse3 = InputProtocol::deserialize(dispatchedPackets[2]);
    ASSERT_TRUE(parse3.hasValue());
    EXPECT_TRUE(std::holds_alternative<KeyEvent>(parse3.value().second));
    const auto& ke = std::get<KeyEvent>(parse3.value().second);
    EXPECT_EQ(ke.virtualKey, 0x42u);
    EXPECT_EQ(ke.action, KeyAction::Press);

    // Verify 4: Gamepad state deserialization
    auto parse4 = InputProtocol::deserialize(dispatchedPackets[3]);
    ASSERT_TRUE(parse4.hasValue());
    EXPECT_TRUE(std::holds_alternative<GamepadState>(parse4.value().second));
    const auto& gs = std::get<GamepadState>(parse4.value().second);
    EXPECT_EQ(gs.index, 1u);
    EXPECT_EQ(gs.buttons, 0x1000u);
    EXPECT_EQ(gs.leftTrigger, 200u);

    // Verify 5: Clipboard event deserialization
    auto parse5 = InputProtocol::deserialize(dispatchedPackets[4]);
    ASSERT_TRUE(parse5.hasValue());
    EXPECT_TRUE(std::holds_alternative<ClipboardEvent>(parse5.value().second));
    const auto& cb = std::get<ClipboardEvent>(parse5.value().second);
    EXPECT_EQ(cb.text, "Cross-platform sync text");

    auto stats = session->getStats();
    EXPECT_EQ(stats.inputPacketsSent, 5u);

    // Disconnect
    EXPECT_TRUE(session->disconnect().hasValue());

    // Sending input while disconnected must fail
    EXPECT_FALSE(session->sendMouseMove(1, 1).hasValue());
}

TEST(PlatformCapabilityMatrixTest, CapabilityDiscoveryAcrossAllPlatforms) {
    auto matrix = VideoDecoderFactory::getCapabilityMatrix();
    ASSERT_EQ(matrix.size(), 5u);

    // 1. Windows D3D11VA
    auto win = matrix[0];
    EXPECT_EQ(win.platform, ClientPlatform::Windows_D3D11VA);
    EXPECT_EQ(win.nativeApi, "Direct3D 11 Video Acceleration (D3D11VA)");
#if defined(_WIN32)
    EXPECT_TRUE(win.isNativeCompiled);
    EXPECT_GE(static_cast<uint8_t>(win.qualificationState),
              static_cast<uint8_t>(DecoderQualificationState::NativeApiAvailable));
#else
    EXPECT_FALSE(win.isNativeCompiled);
    EXPECT_EQ(win.qualificationState, DecoderQualificationState::NotBuilt);
#endif

    // 2. macOS VideoToolbox
    auto mac = matrix[1];
    EXPECT_EQ(mac.platform, ClientPlatform::MacOS_VideoToolbox);
    EXPECT_EQ(mac.nativeApi, "Apple VideoToolbox Framework");
#if defined(__APPLE__)
    EXPECT_TRUE(mac.isNativeCompiled);
    EXPECT_GE(static_cast<uint8_t>(mac.qualificationState),
              static_cast<uint8_t>(DecoderQualificationState::NativeApiAvailable));
#else
    EXPECT_FALSE(mac.isNativeCompiled);
    EXPECT_EQ(mac.qualificationState, DecoderQualificationState::NotBuilt);
#endif

    // 3. Linux VA-API
    auto linuxCap = matrix[2];
    EXPECT_EQ(linuxCap.platform, ClientPlatform::Linux_VaApi);
    EXPECT_EQ(linuxCap.nativeApi, "Linux Video Acceleration API (VA-API / DMA-BUF)");
#if defined(__linux__) && !defined(__ANDROID__)
    EXPECT_TRUE(linuxCap.isNativeCompiled);
    EXPECT_GE(static_cast<uint8_t>(linuxCap.qualificationState),
              static_cast<uint8_t>(DecoderQualificationState::NativeApiAvailable));
#else
    EXPECT_FALSE(linuxCap.isNativeCompiled);
    EXPECT_EQ(linuxCap.qualificationState, DecoderQualificationState::NotBuilt);
#endif

    // 4. Android AMediaCodec
    auto androidCap = matrix[3];
    EXPECT_EQ(androidCap.platform, ClientPlatform::Android_MediaCodec);
    EXPECT_EQ(androidCap.nativeApi, "Android NDK AMediaCodec");
#if defined(__ANDROID__)
    EXPECT_TRUE(androidCap.isNativeCompiled);
    EXPECT_GE(static_cast<uint8_t>(androidCap.qualificationState),
              static_cast<uint8_t>(DecoderQualificationState::NativeApiAvailable));
#else
    EXPECT_FALSE(androidCap.isNativeCompiled);
    EXPECT_EQ(androidCap.qualificationState, DecoderQualificationState::NotBuilt);
#endif

    // 5. Web WebCodecs
    auto webCap = matrix[4];
    EXPECT_EQ(webCap.platform, ClientPlatform::Web_WebRtcWasm);
    EXPECT_EQ(webCap.nativeApi, "W3C WebCodecs / WebAssembly");
#if defined(__EMSCRIPTEN__)
    EXPECT_TRUE(webCap.isNativeCompiled);
    EXPECT_GE(static_cast<uint8_t>(webCap.qualificationState),
              static_cast<uint8_t>(DecoderQualificationState::NativeApiAvailable));
#else
    EXPECT_FALSE(webCap.isNativeCompiled);
    EXPECT_EQ(webCap.qualificationState, DecoderQualificationState::NotBuilt);
#endif
}


TEST(GuestClientSessionLifecycleTest, CompleteStateTransitionsAndRestrictions) {
    GuestClientConfig config{
        .platform = ClientPlatform::Windows_D3D11VA,
        .sessionId = "session-state-test",
        .authToken = "auth-token-12345"
    };

    auto sessionRes = GuestClientSession::create(config);
    ASSERT_TRUE(sessionRes.hasValue());
    auto session = std::move(sessionRes.value());

    EXPECT_EQ(session->getLifecycleState(), SessionLifecycleState::Disconnected);
    EXPECT_FALSE(session->isRelativeMouseEnabled());

    // Pairing submission
    EXPECT_FALSE(session->submitPairingCode("12").hasValue()); // Too short
    EXPECT_TRUE(session->submitPairingCode("ABC123").hasValue());
    EXPECT_EQ(session->getLifecycleState(), SessionLifecycleState::Authenticating);

    // Authentication
    EXPECT_FALSE(session->authenticate("").hasValue());
    EXPECT_TRUE(session->authenticate("valid-secret-token").hasValue());
    EXPECT_EQ(session->getLifecycleState(), SessionLifecycleState::Signaling);

    // Connect
    EXPECT_TRUE(session->connect().hasValue());
    EXPECT_EQ(session->getLifecycleState(), SessionLifecycleState::Connected);

    // Relative mouse toggle
    EXPECT_TRUE(session->setRelativeMouse(true).hasValue());
    EXPECT_TRUE(session->isRelativeMouseEnabled());
    EXPECT_TRUE(session->setRelativeMouse(false).hasValue());
    EXPECT_FALSE(session->isRelativeMouseEnabled());

    // Ingest packet moves to Streaming
    EncodedPacket pkt{};
    pkt.data = {0x00, 0x00, 0x01, 0x67};
    pkt.timestampNs = 1000;
    (void)session->ingestVideoPacket(pkt);
    EXPECT_EQ(session->getLifecycleState(), SessionLifecycleState::Streaming);

    // Reconnection
    EXPECT_TRUE(session->triggerReconnect().hasValue());
    EXPECT_EQ(session->getLifecycleState(), SessionLifecycleState::Connected);

    // Platform restrictions
    auto macRestrictions = GuestClientSession::getPlatformRestrictions(ClientPlatform::MacOS_VideoToolbox);
    EXPECT_FALSE(macRestrictions.empty());
    auto linuxRestrictions = GuestClientSession::getPlatformRestrictions(ClientPlatform::Linux_VaApi);
    EXPECT_FALSE(linuxRestrictions.empty());
    auto webRestrictions = GuestClientSession::getPlatformRestrictions(ClientPlatform::Web_WebRtcWasm);
    EXPECT_FALSE(webRestrictions.empty());

    EXPECT_TRUE(session->disconnect().hasValue());
    EXPECT_EQ(session->getLifecycleState(), SessionLifecycleState::Disconnected);
}

