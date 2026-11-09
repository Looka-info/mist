#include <gtest/gtest.h>
#include "mist/transport/MultiGuestFanout.h"
#include <thread>
#include <atomic>
#include <vector>

using namespace mist;
using namespace mist::transport;
using namespace mist::encode;

class MultiGuestFanoutTest : public ::testing::Test {};

TEST_F(MultiGuestFanoutTest, RegistrationAndLifecycle) {
    MultiGuestFanout fanout;
    EXPECT_EQ(fanout.getGuestCount(), 0u);

    // 1. Invalid empty guest ID rejected
    auto resEmpty = fanout.registerGuestSender("", [](const EncodedPacket&) {
        return common::Result<void>::success();
    });
    EXPECT_TRUE(resEmpty.hasError());
    EXPECT_EQ(resEmpty.error().code(), common::ErrorCode::InvalidArgument);

    // 2. Register valid guests
    std::atomic<uint32_t> guest1Frames{0};
    auto res1 = fanout.registerGuestSender("guest-1", [&guest1Frames](const EncodedPacket&) {
        guest1Frames++;
        return common::Result<void>::success();
    });
    EXPECT_TRUE(res1.hasValue());
    EXPECT_TRUE(fanout.hasGuest("guest-1"));
    EXPECT_EQ(fanout.getGuestCount(), 1u);

    // 3. Duplicate guest registration rejected
    auto resDup = fanout.registerGuestSender("guest-1", [](const EncodedPacket&) {
        return common::Result<void>::success();
    });
    EXPECT_TRUE(resDup.hasError());
    EXPECT_EQ(resDup.error().code(), common::ErrorCode::AlreadyExists);

    // 4. Register second guest
    auto res2 = fanout.registerGuestSender("guest-2", [](const EncodedPacket&) {
        return common::Result<void>::success();
    });
    EXPECT_TRUE(res2.hasValue());
    EXPECT_EQ(fanout.getGuestCount(), 2u);

    auto ids = fanout.getGuestIds();
    EXPECT_EQ(ids.size(), 2u);

    // 5. Unregister guest
    auto resUnreg = fanout.unregisterGuest("guest-1");
    EXPECT_TRUE(resUnreg.hasValue());
    EXPECT_FALSE(fanout.hasGuest("guest-1"));
    EXPECT_EQ(fanout.getGuestCount(), 1u);

    // 6. Unregister non-existent guest
    auto resUnregNone = fanout.unregisterGuest("ghost-guest");
    EXPECT_TRUE(resUnregNone.hasError());
    EXPECT_EQ(resUnregNone.error().code(), common::ErrorCode::NotFound);
}

TEST_F(MultiGuestFanoutTest, EncodeOnceBroadcastToMultipleGuests) {
    MultiGuestFanout fanout;

    std::atomic<uint32_t> g1Count{0};
    std::atomic<uint32_t> g2Count{0};
    std::atomic<uint32_t> g3Count{0};
    std::atomic<uint32_t> g4Count{0};

    EXPECT_TRUE(fanout.registerGuestSender("guest-alpha", [&g1Count](const EncodedPacket&) {
        g1Count++;
        return common::Result<void>::success();
    }).hasValue());

    EXPECT_TRUE(fanout.registerGuestSender("guest-beta", [&g2Count](const EncodedPacket&) {
        g2Count++;
        return common::Result<void>::success();
    }).hasValue());

    EXPECT_TRUE(fanout.registerGuestSender("guest-gamma", [&g3Count](const EncodedPacket&) {
        g3Count++;
        return common::Result<void>::success();
    }).hasValue());

    EXPECT_TRUE(fanout.registerGuestSender("guest-delta", [&g4Count](const EncodedPacket&) {
        g4Count++;
        return common::Result<void>::success();
    }).hasValue());

    EXPECT_EQ(fanout.getGuestCount(), 4u);

    // Broadcast 15 single-encoded frames
    for (uint64_t i = 1; i <= 15; i++) {
        EncodedPacket pkt{};
        pkt.frameNumber = i;
        pkt.timestampNs = i * 16666666ULL;
        pkt.isKeyframe = (i == 1);
        pkt.data = {0x00, 0x00, 0x00, 0x01, 0x65, 0xAA, 0xBB, 0xCC};

        auto bRes = fanout.broadcastVideoFrame(pkt);
        EXPECT_TRUE(bRes.hasValue());
    }

    EXPECT_EQ(g1Count.load(), 15u);
    EXPECT_EQ(g2Count.load(), 15u);
    EXPECT_EQ(g3Count.load(), 15u);
    EXPECT_EQ(g4Count.load(), 15u);

    auto snap = fanout.getSnapshot();
    EXPECT_EQ(snap.totalGuests, 4u);
    EXPECT_EQ(snap.activeGuests, 4u);
    EXPECT_EQ(snap.totalFramesBroadcast, 15u);
    EXPECT_EQ(snap.totalFramesDelivered, 60u);
    EXPECT_EQ(snap.totalFramesDropped, 0u);
    EXPECT_EQ(snap.slowGuests, 0u);
}

TEST_F(MultiGuestFanoutTest, SlowClientIsolationAndQueueBounding) {
    MultiGuestFanout fanout(2 /*maxQueueDepth*/);

    std::atomic<uint32_t> fastClientReceived{0};
    std::atomic<uint32_t> slowClientReceived{0};

    EXPECT_TRUE(fanout.registerGuestSender("fast-client", [&fastClientReceived](const EncodedPacket&) {
        fastClientReceived++;
        return common::Result<void>::success();
    }).hasValue());

    EXPECT_TRUE(fanout.registerGuestSender("slow-client", [&slowClientReceived](const EncodedPacket&) {
        slowClientReceived++;
        return common::Result<void>::success();
    }).hasValue());

    // 1. Initial 3 frames when both clients are healthy
    for (int i = 0; i < 3; i++) {
        EncodedPacket pkt{};
        pkt.data = {0x01, 0x02};
        EXPECT_TRUE(fanout.broadcastVideoFrame(pkt).hasValue());
    }
    EXPECT_EQ(fastClientReceived.load(), 3u);
    EXPECT_EQ(slowClientReceived.load(), 3u);

    // 2. Slow client network buffers saturate (simulated queue depth >= maxQueueDepth)
    fanout.setGuestSimulatedQueueDepth("slow-client", 2);

    for (int i = 0; i < 5; i++) {
        EncodedPacket pkt{};
        pkt.data = {0x01, 0x02};
        EXPECT_TRUE(fanout.broadcastVideoFrame(pkt).hasValue());
    }

    // Fast client continues receiving 100% of frames in real-time
    EXPECT_EQ(fastClientReceived.load(), 8u);
    // Slow client has its frames dropped to preserve bounded latency
    EXPECT_EQ(slowClientReceived.load(), 3u);

    auto slowStats = fanout.getGuestStats("slow-client").value();
    EXPECT_EQ(slowStats.framesSent, 3u);
    EXPECT_EQ(slowStats.framesDropped, 5u);
    EXPECT_TRUE(slowStats.isSlowClient);

    auto fastStats = fanout.getGuestStats("fast-client").value();
    EXPECT_EQ(fastStats.framesSent, 8u);
    EXPECT_EQ(fastStats.framesDropped, 0u);
    EXPECT_FALSE(fastStats.isSlowClient);

    // 3. Slow client recovers and drains queue
    fanout.setGuestSimulatedQueueDepth("slow-client", 0);

    for (int i = 0; i < 4; i++) {
        EncodedPacket pkt{};
        pkt.data = {0x01, 0x02};
        EXPECT_TRUE(fanout.broadcastVideoFrame(pkt).hasValue());
    }

    EXPECT_EQ(fastClientReceived.load(), 12u);
    EXPECT_EQ(slowClientReceived.load(), 7u);

    auto recoveredStats = fanout.getGuestStats("slow-client").value();
    EXPECT_FALSE(recoveredStats.isSlowClient);
    EXPECT_EQ(recoveredStats.consecutiveDrops, 0u);
}

TEST_F(MultiGuestFanoutTest, KeyframeDebouncingAcrossMultipleGuests) {
    // Set debounce window to 150ms
    MultiGuestFanout fanout(2, 150);

    std::atomic<uint32_t> keyframeTriggerCount{0};
    fanout.setKeyframeRequestCallback([&keyframeTriggerCount]() {
        keyframeTriggerCount++;
    });

    // 1. First keyframe request from Guest A immediately triggers
    fanout.requestKeyframe("guest-A");
    EXPECT_EQ(keyframeTriggerCount.load(), 1u);

    // 2. Immediate requests from Guests B, C, D within 150ms are debounced
    fanout.requestKeyframe("guest-B");
    fanout.requestKeyframe("guest-C");
    fanout.requestKeyframe("guest-D");
    EXPECT_EQ(keyframeTriggerCount.load(), 1u);

    // 3. After debounce window elapses, next keyframe fires
    std::this_thread::sleep_for(std::chrono::milliseconds(160));
    fanout.requestKeyframe("guest-B");
    EXPECT_EQ(keyframeTriggerCount.load(), 2u);
}
