#include <gtest/gtest.h>
#include "mist/transport/WebRtcTransport.h"
#include <thread>
#include <chrono>

using namespace mist;
using namespace mist::transport;

TEST(TestNetworkStability, MultipleOpenCloseCycles) {
    WebRtcTransportConfig config{};
    config.isHost = true;
    config.sessionId = "stability-cycle-session";

    for (int i = 0; i < 3; ++i) {
        auto transport = WebRtcTransport::create();
        ASSERT_NE(transport, nullptr);
        auto initRes = transport->initializeWebRtc(config);
        EXPECT_TRUE(initRes.hasValue());

        auto discRes = transport->disconnect();
        EXPECT_TRUE(discRes.hasValue());
        EXPECT_FALSE(transport->isConnected());
    }
}

TEST(TestNetworkStability, BackpressureDropOldestFrames) {
    WebRtcTransportConfig config{};
    config.isHost = true;
    config.sessionId = "backpressure-session";

    auto transport = WebRtcTransport::create();
    ASSERT_TRUE(transport->initializeWebRtc(config).hasValue());

    // When connection is not yet open, sendVideoPacket should drop cleanly without blocking or leaking
    encode::EncodedPacket pkt{};
    pkt.data = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1f};
    pkt.isKeyframe = true;
    pkt.timestampNs = 1000;

    for (int i = 0; i < 20; ++i) {
        pkt.frameNumber = static_cast<uint64_t>(i);
        auto res = transport->sendVideoPacket(pkt);
        EXPECT_TRUE(res.hasValue());
    }

    auto sender = transport->getVideoSender();
    ASSERT_NE(sender, nullptr);
    EXPECT_EQ(sender->getFramesSent(), 0u);
    EXPECT_EQ(sender->getFramesDropped(), 20u);
}

TEST(TestNetworkStability, TransportStatisticsUnderLoad) {
    TransportStatistics stats;
    const size_t iterations = 1000;

    for (size_t i = 0; i < iterations; ++i) {
        stats.recordPacketSent();
        stats.recordBytesSent(1200);
        stats.recordPacketReceived();
        stats.recordBytesReceived(1200);
    }

    auto snap = stats.getSnapshot();
    EXPECT_EQ(snap.packetsSent, iterations);
    EXPECT_EQ(snap.bytesSent, iterations * 1200);
    EXPECT_EQ(snap.packetsReceived, iterations);
    EXPECT_EQ(snap.bytesReceived, iterations * 1200);
}
