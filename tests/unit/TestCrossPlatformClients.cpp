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
    EXPECT_TRUE(decoder->initialize(cfg).hasValue());

    for (uint32_t i = 1; i <= 4; ++i) {
        EncodedPacket pkt{};
        pkt.data = {0x00, 0x00, 0x00, 0x01, static_cast<uint8_t>(i)};
        pkt.timestampNs = i * 16666666ULL;
        EXPECT_TRUE(decoder->submitPacket(pkt).hasValue());

        auto frameRes = decoder->getNextFrame();
        ASSERT_TRUE(frameRes.hasValue());
        EXPECT_EQ(frameRes.value().frameNumber, i);
        // Verify simulated surface ID cycle (1, 2, 3, 4, ...)
        uintptr_t surfaceId = reinterpret_cast<uintptr_t>(frameRes.value().nativeResource);
        EXPECT_EQ(surfaceId, ((i % 4) + 1));
        EXPECT_TRUE(decoder->releaseFrame(frameRes.value()).hasValue());
    }

    EXPECT_EQ(decoder->getDecodedFrameCount(), 4u);
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
    EXPECT_TRUE(decoder->initialize(cfg).hasValue());

    // Empty packet submission should fail validation
    EncodedPacket emptyPkt{};
    EXPECT_FALSE(decoder->submitPacket(emptyPkt).hasValue());

    // Valid packet submission
    EncodedPacket validPkt{};
    validPkt.data = {0x00, 0x00, 0x01, 0x65, 0x88, 0x84};
    validPkt.timestampNs = 555555ULL;
    EXPECT_TRUE(decoder->submitPacket(validPkt).hasValue());

    auto frameRes = decoder->getNextFrame();
    ASSERT_TRUE(frameRes.hasValue());
    EXPECT_EQ(frameRes.value().timestampNs, 555555ULL);
    EXPECT_TRUE(decoder->releaseFrame(frameRes.value()).hasValue());
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
    EXPECT_TRUE(decoder->initialize(cfg).hasValue());

    EncodedPacket pkt{};
    pkt.data = {0x0A, 0x08, 0x00, 0x00};
    pkt.timestampNs = 9999ULL;
    EXPECT_TRUE(decoder->submitPacket(pkt).hasValue());

    auto frameRes = decoder->getNextFrame();
    ASSERT_TRUE(frameRes.hasValue());
    EXPECT_TRUE(decoder->flush().hasValue());
    EXPECT_TRUE(decoder->shutdown().hasValue());
}

TEST(GuestClientSessionTest, LifecycleAndVideoIngestion) {
    GuestClientConfig config{
        .platform = ClientPlatform::MacOS_VideoToolbox,
        .sessionId = "session-test-42",
        .preferredCodec = VideoCodec::AV1,
        .enableAudio = true,
        .enableInput = true
    };

    auto sessionRes = GuestClientSession::create(config);
    ASSERT_TRUE(sessionRes.hasValue());
    auto session = std::move(sessionRes.value());

    EXPECT_FALSE(session->isConnected());
    EXPECT_EQ(session->getPlatform(), ClientPlatform::MacOS_VideoToolbox);

    bool connectionStateObserved = false;
    session->setConnectionStateCallback([&](bool connected) {
        connectionStateObserved = connected;
    });

    uint32_t framesReceivedCount = 0;
    session->setFrameDecodedCallback([&](const DecodedFrame& frame) {
        ++framesReceivedCount;
        EXPECT_EQ(frame.width, 1920u);
        EXPECT_EQ(frame.height, 1080u);
    });

    EXPECT_TRUE(session->connect().hasValue());
    EXPECT_TRUE(session->isConnected());
    EXPECT_TRUE(connectionStateObserved);

    // Ingest video packets
    for (int i = 0; i < 5; ++i) {
        EncodedPacket pkt{};
        pkt.data = {0x12, 0x00, 0xAA, 0xBB};
        pkt.timestampNs = i * 16666666ULL;
        EXPECT_TRUE(session->ingestVideoPacket(pkt).hasValue());
    }

    EXPECT_EQ(framesReceivedCount, 5u);

    // Ingest audio packet
    std::vector<uint8_t> opusPacket = {0xF8, 0xFF, 0xFE};
    EXPECT_TRUE(session->ingestAudioPacket(opusPacket.data(), opusPacket.size(), 1000ULL).hasValue());

    auto stats = session->getStats();
    EXPECT_EQ(stats.videoPacketsReceived, 5u);
    EXPECT_EQ(stats.framesDecoded, 5u);
    EXPECT_EQ(stats.framesPresented, 5u);
    EXPECT_EQ(stats.audioPacketsReceived, 1u);
    EXPECT_GT(stats.averageDecodeLatencyMs, 0.0);
    EXPECT_EQ(stats.activeCodec, "AV1");
    EXPECT_EQ(stats.platformName, "macOS (Apple VideoToolbox)");

    EXPECT_TRUE(session->disconnect().hasValue());
    EXPECT_FALSE(session->isConnected());
    EXPECT_FALSE(connectionStateObserved);
}

TEST(GuestClientSessionTest, InputPackagingAndDispatch) {
    GuestClientConfig config{
        .platform = ClientPlatform::Linux_VaApi,
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
    EXPECT_TRUE(win.isHardwareAccelerated);
    EXPECT_EQ(win.releaseStatus, "Production Qualified");
#endif
    EXPECT_TRUE(win.supportsH264);
    EXPECT_TRUE(win.supportsHevc);
    EXPECT_TRUE(win.supportsAv1);

    // 2. macOS VideoToolbox
    auto mac = matrix[1];
    EXPECT_EQ(mac.platform, ClientPlatform::MacOS_VideoToolbox);
    EXPECT_EQ(mac.nativeApi, "Apple VideoToolbox Framework");
    EXPECT_TRUE(mac.supportsH264);
    EXPECT_TRUE(mac.supportsHevc);
    EXPECT_TRUE(mac.supportsAv1);

    // 3. Linux VA-API
    auto linuxCap = matrix[2];
    EXPECT_EQ(linuxCap.platform, ClientPlatform::Linux_VaApi);
    EXPECT_EQ(linuxCap.nativeApi, "Linux Video Acceleration API (VA-API / DMA-BUF)");
    EXPECT_TRUE(linuxCap.supportsH264);
    EXPECT_TRUE(linuxCap.supportsHevc);
    EXPECT_TRUE(linuxCap.supportsAv1);

    // 4. Android AMediaCodec
    auto androidCap = matrix[3];
    EXPECT_EQ(androidCap.platform, ClientPlatform::Android_MediaCodec);
    EXPECT_EQ(androidCap.nativeApi, "Android NDK AMediaCodec");
    EXPECT_TRUE(androidCap.supportsH264);

    // 5. Web WebCodecs
    auto webCap = matrix[4];
    EXPECT_EQ(webCap.platform, ClientPlatform::Web_WebRtcWasm);
    EXPECT_EQ(webCap.nativeApi, "W3C WebCodecs / WebAssembly");
    EXPECT_TRUE(webCap.supportsH264);
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

