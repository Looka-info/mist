#include <gtest/gtest.h>
#include "mist/transport/ForwardErrorCorrection.h"
#include "mist/transport/AudioPlcController.h"

using namespace mist::transport;

TEST(FecAndRecoveryTest, FecSinglePacketLossRecovery) {
    FecEncoder encoder(4); // 4 media packets per FEC block
    FecDecoder decoder;

    // Create 4 media packets with varying lengths and payloads
    std::vector<std::vector<uint8_t>> mediaPackets = {
        {0x10, 0x11, 0x12, 0x13},
        {0x20, 0x21, 0x22, 0x23, 0x24, 0x25},
        {0x30, 0x31, 0x32},                    // Packet 2 will be "lost"
        {0x40, 0x41, 0x42, 0x43, 0x44}
    };

    uint16_t baseSeq = 100;
    for (size_t i = 0; i < mediaPackets.size(); ++i) {
        encoder.addMediaPacket(static_cast<uint16_t>(baseSeq + i), mediaPackets[i]);
    }

    auto fecPackets = encoder.generatePendingFecPackets();
    ASSERT_EQ(fecPackets.size(), 1u);
    const auto& fec = fecPackets[0];
    EXPECT_EQ(fec.baseSequenceNumber, baseSeq);
    EXPECT_EQ(fec.mask, 0x000F); // 4 packets covered (bits 0, 1, 2, 3)

    // Deliver packets 0, 1, and 3 to decoder (packet 2 is lost)
    decoder.addMediaPacket(baseSeq + 0, mediaPackets[0]);
    decoder.addMediaPacket(baseSeq + 1, mediaPackets[1]);
    // Note: Packet baseSeq + 2 is NOT delivered!
    decoder.addMediaPacket(baseSeq + 3, mediaPackets[3]);

    // Deliver FEC packet
    decoder.addFecPacket(fec);

    // Attempt recovery
    auto recovered = decoder.tryRecoverPackets();
    ASSERT_EQ(recovered.size(), 1u);
    EXPECT_EQ(recovered[0].first, baseSeq + 2); // Recovered packet 2
    EXPECT_EQ(recovered[0].second, mediaPackets[2]); // Exact byte-for-byte match!
    EXPECT_EQ(decoder.getPacketsRecovered(), 1u);
}

TEST(FecAndRecoveryTest, FecMultipleBlocksRecovery) {
    FecEncoder encoder(3);
    FecDecoder decoder;

    std::vector<uint8_t> p0 = {0xAA, 0xBB};
    std::vector<uint8_t> p1 = {0xCC, 0xDD, 0xEE}; // Lost in block 1
    std::vector<uint8_t> p2 = {0x11};

    std::vector<uint8_t> p3 = {0x22, 0x33};
    std::vector<uint8_t> p4 = {0x44, 0x55, 0x66};
    std::vector<uint8_t> p5 = {0x77, 0x88};        // Lost in block 2

    // Block 1
    encoder.addMediaPacket(10, p0);
    encoder.addMediaPacket(11, p1);
    encoder.addMediaPacket(12, p2);
    auto fec1 = encoder.generatePendingFecPackets();
    ASSERT_EQ(fec1.size(), 1u);

    // Block 2
    encoder.addMediaPacket(13, p3);
    encoder.addMediaPacket(14, p4);
    encoder.addMediaPacket(15, p5);
    auto fec2 = encoder.generatePendingFecPackets();
    ASSERT_EQ(fec2.size(), 1u);

    // Deliver received packets
    decoder.addMediaPacket(10, p0);
    // 11 lost
    decoder.addMediaPacket(12, p2);
    decoder.addFecPacket(fec1[0]);

    decoder.addMediaPacket(13, p3);
    decoder.addMediaPacket(14, p4);
    // 15 lost
    decoder.addFecPacket(fec2[0]);

    auto recovered = decoder.tryRecoverPackets();
    ASSERT_EQ(recovered.size(), 2u);
    EXPECT_EQ(recovered[0].first, 11u);
    EXPECT_EQ(recovered[0].second, p1);
    EXPECT_EQ(recovered[1].first, 15u);
    EXPECT_EQ(recovered[1].second, p5);
    EXPECT_EQ(decoder.getPacketsRecovered(), 2u);
}

TEST(FecAndRecoveryTest, FecUnrecoverableLossExceedingParityCapacity) {
    FecEncoder encoder(4);
    FecDecoder decoder;

    std::vector<uint8_t> p0 = {0x01};
    std::vector<uint8_t> p1 = {0x02}; // Lost
    std::vector<uint8_t> p2 = {0x03}; // Also Lost (2 losses in 1 parity group!)
    std::vector<uint8_t> p3 = {0x04};

    encoder.addMediaPacket(100, p0);
    encoder.addMediaPacket(101, p1);
    encoder.addMediaPacket(102, p2);
    encoder.addMediaPacket(103, p3);

    auto fecPackets = encoder.generatePendingFecPackets();
    ASSERT_EQ(fecPackets.size(), 1u);

    decoder.addMediaPacket(100, p0);
    decoder.addMediaPacket(103, p3);
    decoder.addFecPacket(fecPackets[0]);

    // Cannot recover when 2 packets are missing from 1 parity packet
    auto recovered = decoder.tryRecoverPackets();
    EXPECT_TRUE(recovered.empty());
    EXPECT_EQ(decoder.getPacketsRecovered(), 0u);
}

TEST(FecAndRecoveryTest, RapidReferenceRecoveryEvaluationAndDebouncing) {
    RapidReferenceRecovery recovery(200); // 200ms debounce

    uint64_t t0 = 1000000000ULL;

    // 1. First loss event triggers keyframe
    auto ev1 = recovery.requestRecovery("guest-1", RecoveryReason::UnrecoverableLoss, t0);
    EXPECT_TRUE(ev1.isKeyframeTriggered);
    EXPECT_EQ(recovery.getKeyframesDispatched(), 1u);

    // 2. Immediate second request (50ms later) from another loss event is debounced
    uint64_t t1 = t0 + 50000000ULL;
    auto ev2 = recovery.requestRecovery("guest-2", RecoveryReason::UnrecoverableLoss, t1);
    EXPECT_FALSE(ev2.isKeyframeTriggered);
    EXPECT_EQ(recovery.getKeyframesDispatched(), 1u);

    // 3. New guest joining bypasses debounce to immediately receive video
    auto ev3 = recovery.requestRecovery("guest-3", RecoveryReason::NewGuestJoined, t1);
    EXPECT_TRUE(ev3.isKeyframeTriggered);
    EXPECT_EQ(recovery.getKeyframesDispatched(), 2u);

    // 4. Request after debounce interval (250ms later) triggers keyframe
    uint64_t t2 = t1 + 250000000ULL;
    auto ev4 = recovery.requestRecovery("guest-1", RecoveryReason::DecoderDesync, t2);
    EXPECT_TRUE(ev4.isKeyframeTriggered);
    EXPECT_EQ(recovery.getKeyframesDispatched(), 3u);
    EXPECT_EQ(recovery.getTotalRecoveryRequests(), 4u);
}

TEST(FecAndRecoveryTest, AudioPlcGapDetectionAndConcealment) {
    AudioPlcController plcController(960); // 960 samples per 20ms frame

    uint16_t callbackLostCount = 0;
    uint32_t callbackEstimatedSamples = 0;
    plcController.setPacketLostCallback([&](uint16_t lost, uint32_t samples) {
        callbackLostCount = lost;
        callbackEstimatedSamples = samples;
    });

    // Packet 50 arrives
    plcController.onAudioPacketReceived(50, 10000);
    EXPECT_EQ(callbackLostCount, 0u);
    EXPECT_EQ(plcController.getPacketsLost(), 0u);

    // Packet 51 arrives (continuous)
    plcController.onAudioPacketReceived(51, 10960);
    EXPECT_EQ(callbackLostCount, 0u);
    EXPECT_EQ(plcController.getPacketsLost(), 0u);

    // Packet 54 arrives (missing 52 and 53 = 2 lost packets)
    plcController.onAudioPacketReceived(54, 13840);
    EXPECT_EQ(callbackLostCount, 2u);
    EXPECT_EQ(callbackEstimatedSamples, 1920u); // 2 * 960
    EXPECT_EQ(plcController.getPacketsLost(), 2u);
    EXPECT_EQ(plcController.getPlcConcealmentsTriggered(), 2u);

    // Out-of-order duplicate packet 53 arriving late should not trigger PLC
    callbackLostCount = 0;
    plcController.onAudioPacketReceived(53, 12880);
    EXPECT_EQ(callbackLostCount, 0u);
}
