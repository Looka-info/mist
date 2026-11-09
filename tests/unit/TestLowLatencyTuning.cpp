#include <gtest/gtest.h>
#include "mist/transport/LowLatencyTuning.h"
#include "mist/transport/PacedSender.h"
#include "mist/transport/NackPliController.h"

using namespace mist::transport;

TEST(LowLatencyTuningTest, PlayoutDelaySdpInjectionAndParsing) {
    std::string sdp = 
        "v=0\r\n"
        "o=- 12345 2 IN IP4 127.0.0.1\r\n"
        "s=-\r\n"
        "t=0 0\r\n"
        "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=rtpmap:96 H264/90000\r\n";

    // Inject playout delay: min 0 frames, max 1 frame @ 60fps (0ms - 16ms)
    std::string modifiedSdp = LowLatencyTuner::injectPlayoutDelay(sdp, 0, 1, 60);
    EXPECT_NE(modifiedSdp.find("http://www.webrtc.org/experiments/rtp-hdrext/playout-delay"), std::string::npos);
    EXPECT_NE(modifiedSdp.find("a=playout-delay:0 16"), std::string::npos);

    auto [minMs, maxMs] = LowLatencyTuner::parsePlayoutDelayMs(modifiedSdp);
    EXPECT_EQ(minMs, 0u);
    EXPECT_EQ(maxMs, 16u);
}

TEST(LowLatencyTuningTest, OptimalRtcpIntervalCalculation) {
    // Low latency local LAN: RTT = 10ms -> 50ms interval
    uint32_t lanInterval = LowLatencyTuner::calculateOptimalRtcpIntervalMs(10, 60, 1);
    EXPECT_EQ(lanInterval, 50u);

    // Medium RTT (45ms) -> 75ms interval
    uint32_t mediumInterval = LowLatencyTuner::calculateOptimalRtcpIntervalMs(45, 60, 1);
    EXPECT_EQ(mediumInterval, 75u);

    // High RTT (100ms) -> 100ms interval
    uint32_t highInterval = LowLatencyTuner::calculateOptimalRtcpIntervalMs(100, 60, 1);
    EXPECT_EQ(highInterval, 100u);

    // Multiple guests scale interval to conserve upload bandwidth
    uint32_t multiGuestInterval = LowLatencyTuner::calculateOptimalRtcpIntervalMs(10, 60, 4);
    EXPECT_EQ(multiGuestInterval, 65u);
}

TEST(LowLatencyTuningTest, PacedSenderPriorityAndTokenBucket) {
    LowLatencyProfile profile{};
    profile.enablePacedSender = true;
    profile.pacingFactor = 1.0; // Exact bitrate
    profile.maxQueueDurationMs = 33; // 2 frames @ 60fps
    profile.maxQueuePackets = 100;

    PacedSender pacer(profile);
    pacer.setPacingRate(8000); // 8000 kbps = 1,000,000 bytes/sec = 1 byte/microsecond

    uint64_t t0 = 1000000000ULL; // 1.0s

    // 1. Enqueue high-priority audio packet and video packet
    std::vector<uint8_t> audioPkt(100, 0xAA);
    std::vector<uint8_t> videoPkt(1000, 0xBB);

    EXPECT_TRUE(pacer.enqueuePacket(videoPkt, t0, /*isHighPriority=*/false));
    EXPECT_TRUE(pacer.enqueuePacket(audioPkt, t0, /*isHighPriority=*/true));

    // High-priority audio should be dequeued first even though video was enqueued first
    auto popped1 = pacer.getNextPacketToTransmit(t0);
    ASSERT_TRUE(popped1.has_value());
    EXPECT_EQ(popped1.value().size(), 100u);
    EXPECT_EQ(popped1.value()[0], 0xAA);

    // Next, video packet should be dequeued
    auto popped2 = pacer.getNextPacketToTransmit(t0);
    ASSERT_TRUE(popped2.has_value());
    EXPECT_EQ(popped2.value().size(), 1000u);
    EXPECT_EQ(popped2.value()[0], 0xBB);
}

TEST(LowLatencyTuningTest, PacedSenderStalePacketDropping) {
    LowLatencyProfile profile{};
    profile.enablePacedSender = true;
    profile.maxQueueDurationMs = 33; // Max 33ms latency bound

    PacedSender pacer(profile);
    pacer.setPacingRate(10000);

    uint64_t t0 = 1000000000ULL;
    std::vector<uint8_t> videoPkt(500, 0x11);
    EXPECT_TRUE(pacer.enqueuePacket(videoPkt, t0, false));

    // Advance time by 50ms (> 33ms limit)
    uint64_t t1 = t0 + 50000000ULL;
    auto popped = pacer.getNextPacketToTransmit(t1);

    // Stale packet should have been dropped
    EXPECT_FALSE(popped.has_value());
    EXPECT_EQ(pacer.getPacketsDropped(), 1u);
}

TEST(LowLatencyTuningTest, NackPliFastRecoveryUnderLowRtt) {
    LowLatencyProfile profile{};
    profile.enableFastNack = true;
    profile.nackRttThresholdMs = 60;
    profile.pliDebounceIntervalMs = 200;

    NackPliController controller(profile);

    uint64_t now = 1000000000ULL;

    // Receive packet 100
    auto d1 = controller.onRtpPacketReceived(100, 20, now);
    EXPECT_FALSE(d1.shouldSendNack);
    EXPECT_FALSE(d1.shouldSendPli);

    // Receive packet 103 (missing 101, 102) with low RTT (20ms < 60ms)
    auto d2 = controller.onRtpPacketReceived(103, 20, now + 16000000ULL);
    EXPECT_TRUE(d2.shouldSendNack);
    EXPECT_FALSE(d2.shouldSendPli);
    ASSERT_EQ(d2.missingSequenceNumbers.size(), 2u);
    EXPECT_EQ(d2.missingSequenceNumbers[0], 101);
    EXPECT_EQ(d2.missingSequenceNumbers[1], 102);
    EXPECT_EQ(controller.getNacksGenerated(), 2u);
}

TEST(LowLatencyTuningTest, NackPliEscalationToPliUnderHighRttAndDebouncing) {
    LowLatencyProfile profile{};
    profile.enableFastNack = true;
    profile.nackRttThresholdMs = 60;
    profile.pliDebounceIntervalMs = 200;

    NackPliController controller(profile);

    uint64_t now = 1000000000ULL;
    (void)controller.onRtpPacketReceived(500, 80, now);

    // Packet gap at 80ms RTT (> 60ms) should suppress NACK and trigger PLI
    auto d1 = controller.onRtpPacketReceived(505, 80, now + 16000000ULL);
    EXPECT_FALSE(d1.shouldSendNack);
    EXPECT_TRUE(d1.shouldSendPli);
    EXPECT_EQ(controller.getPlisRequested(), 1u);
    EXPECT_EQ(controller.getNacksSuppressed(), 4u); // 501, 502, 503, 504

    // Immediate second loss gap within 200ms debounce window (e.g. +50ms)
    auto d2 = controller.onRtpPacketReceived(510, 80, now + 66000000ULL);
    EXPECT_FALSE(d2.shouldSendNack);
    EXPECT_FALSE(d2.shouldSendPli); // Debounced!
    EXPECT_EQ(controller.getPlisSuppressed(), 1u);

    // Loss gap after 250ms (> 200ms debounce window) allows fresh PLI
    auto d3 = controller.onRtpPacketReceived(515, 80, now + 300000000ULL);
    EXPECT_TRUE(d3.shouldSendPli);
    EXPECT_EQ(controller.getPlisRequested(), 2u);
}
