#include <gtest/gtest.h>
#include "mist/transport/AudioTrackSender.h"
#include "mist/transport/AudioTrackReceiver.h"
#include "mist/transport/WebRtcTransport.h"
#include "rtc/rtc.hpp"

using namespace mist::transport;

TEST(AudioWebRtcTest, AudioTrackSenderAndReceiverLifecycle) {
    WebRtcTransportConfig config{};
    config.enableAudio = true;

    AudioTrackReceiver receiver;
    EXPECT_TRUE(receiver.initialize(config).hasValue());
    EXPECT_FALSE(receiver.isTrackOpen());
    EXPECT_EQ(receiver.getPacketsReceived(), 0u);

    bool received = false;
    receiver.setFrameCallback([&](std::span<const uint8_t> /*payload*/, const AudioFrameInfo& /*info*/) {
        received = true;
    });

    receiver.close();
}

TEST(AudioWebRtcTest, WebRtcTransportAudioConfiguration) {
    auto transport = WebRtcTransport::create();
    WebRtcTransportConfig config{};
    config.enableAudio = true;
    config.sessionId = "audio-test-session";

    auto initRes = transport->initializeWebRtc(config);
    EXPECT_TRUE(initRes.hasValue());

    // Host should have audioSender_
    EXPECT_NE(transport->getAudioSender(), nullptr);
    EXPECT_EQ(transport->getAudioReceiver(), nullptr);

    // Audio frame send check (track not connected yet -> expect InvalidState or handled)
    std::vector<uint8_t> testOpus(100, 0xAA);
    auto sendRes = transport->sendAudioFrame(testOpus, 480);
    EXPECT_TRUE(sendRes.hasError()); // track not open

    (void)transport->disconnect();
}

TEST(AudioWebRtcTest, ClientWebRtcTransportAudioSetup) {
    auto clientTransport = WebRtcTransport::create();
    WebRtcTransportConfig clientConfig{};
    clientConfig.isHost = false;
    clientConfig.enableAudio = true;

    auto initRes = clientTransport->initializeWebRtc(clientConfig);
    EXPECT_TRUE(initRes.hasValue());

    EXPECT_NE(clientTransport->getAudioReceiver(), nullptr);
    EXPECT_EQ(clientTransport->getAudioSender(), nullptr);

    (void)clientTransport->disconnect();
}
