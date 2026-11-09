#include <gtest/gtest.h>
#include "mist/transport/qos/PerGuestAdaptiveBitrate.h"

using namespace mist;
using namespace mist::transport::qos;

class PerGuestAdaptiveBitrateTest : public ::testing::Test {};

TEST_F(PerGuestAdaptiveBitrateTest, RegistrationAndInitialTargets) {
    PerGuestAdaptiveBitrateManager manager(2);
    EXPECT_EQ(manager.getGuestCount(), 0u);

    // 1. Invalid empty guest ID
    auto resEmpty = manager.registerGuest("");
    EXPECT_TRUE(resEmpty.hasError());
    EXPECT_EQ(resEmpty.error().code(), common::ErrorCode::InvalidArgument);

    // 2. Register guests
    EXPECT_TRUE(manager.registerGuest("guest-lan", QualityProfile::Quality).hasValue());
    EXPECT_TRUE(manager.registerGuest("guest-wifi", QualityProfile::Adaptive).hasValue());
    EXPECT_EQ(manager.getGuestCount(), 2u);

    // 3. Duplicate registration rejected
    auto resDup = manager.registerGuest("guest-lan");
    EXPECT_TRUE(resDup.hasError());
    EXPECT_EQ(resDup.error().code(), common::ErrorCode::AlreadyExists);

    // Query targets
    auto targetLan = manager.getGuestTarget("guest-lan").value();
    EXPECT_EQ(targetLan.guestId, "guest-lan");
    EXPECT_EQ(targetLan.width, 1920u);
    EXPECT_EQ(targetLan.height, 1080u);

    auto targetWifi = manager.getGuestTarget("guest-wifi").value();
    EXPECT_EQ(targetWifi.guestId, "guest-wifi");

    // Unregister
    EXPECT_TRUE(manager.unregisterGuest("guest-wifi").hasValue());
    EXPECT_EQ(manager.getGuestCount(), 1u);
}

TEST_F(PerGuestAdaptiveBitrateTest, DynamicAdaptationUnderNetworkCongestion) {
    PerGuestAdaptiveBitrateManager manager(2);

    EXPECT_TRUE(manager.registerGuest("guest-fast", QualityProfile::Adaptive).hasValue());
    EXPECT_TRUE(manager.registerGuest("guest-slow", QualityProfile::Adaptive).hasValue());

    // Telemetry for guest-fast: excellent LAN (0% loss, 10ms RTT, 20Mbps REMB)
    RtcpStatistics statsFast{};
    statsFast.fractionLost = 0; // 0%
    statsFast.roundTripTimeMs = 10.0;
    statsFast.receiverEstimatedMaxBitrateBps = 20000000; // 20 Mbps

    manager.onGuestRtcp("guest-fast", statsFast, 1000);
    manager.onGuestRemb("guest-fast", 20000000, 1000);

    // Telemetry for guest-slow: congested Wi-Fi / cellular (9.3% loss, 120ms RTT, 3.5Mbps REMB)
    RtcpStatistics statsSlow{};
    statsSlow.fractionLost = 24; // 24 / 256 ≈ 9.375%
    statsSlow.roundTripTimeMs = 120.0;
    statsSlow.receiverEstimatedMaxBitrateBps = 3500000; // 3.5 Mbps

    manager.onGuestRtcp("guest-slow", statsSlow, 1000);
    manager.onGuestRemb("guest-slow", 3500000, 1000);

    // Evaluate adaptation
    auto tickRes = manager.tick(1500);
    EXPECT_TRUE(tickRes.hasValue());

    auto targetFast = manager.getGuestTarget("guest-fast").value();
    auto targetSlow = manager.getGuestTarget("guest-slow").value();

    // Fast guest gets 1080p Quality tier
    EXPECT_EQ(targetFast.width, 1920u);
    EXPECT_EQ(targetFast.height, 1080u);
    EXPECT_EQ(targetFast.renditionIndex, 0u);
    EXPECT_EQ(targetFast.profile, QualityProfile::Quality);

    // Slow guest is dynamically downgraded to 720p Performance tier
    EXPECT_EQ(targetSlow.width, 1280u);
    EXPECT_EQ(targetSlow.height, 720u);
    EXPECT_EQ(targetSlow.renditionIndex, 1u);
    EXPECT_EQ(targetSlow.profile, QualityProfile::Performance);
}

TEST_F(PerGuestAdaptiveBitrateTest, MultiRenditionClusteringAndCallbacks) {
    PerGuestAdaptiveBitrateManager manager(2);

    std::vector<std::pair<std::string, uint32_t>> migrations;
    manager.setGuestMigrationCallback([&migrations](const std::string& id, uint32_t /*oldR*/, uint32_t newR) {
        migrations.emplace_back(id, newR);
    });

    EXPECT_TRUE(manager.registerGuest("guest-A", QualityProfile::Adaptive).hasValue());
    EXPECT_TRUE(manager.registerGuest("guest-B", QualityProfile::Adaptive).hasValue());

    // Degrade guest-B
    RtcpStatistics statsB{};
    statsB.fractionLost = 25; // ~9.7%
    statsB.roundTripTimeMs = 110.0;
    manager.onGuestRtcp("guest-B", statsB, 2000);
    manager.onGuestRemb("guest-B", 3000000, 2000);

    EXPECT_TRUE(manager.tick(2500).hasValue());

    // Verify migration callback fired for guest-B to Rendition 1
    EXPECT_FALSE(migrations.empty());
    EXPECT_EQ(migrations.back().first, "guest-B");
    EXPECT_EQ(migrations.back().second, 1u);

    // Active renditions inspect
    auto renditions = manager.getActiveRenditions();
    EXPECT_EQ(renditions.size(), 2u);
    EXPECT_EQ(renditions[0].width, 1920u);
    EXPECT_EQ(renditions[0].assignedGuestCount, 1u); // guest-A
    EXPECT_EQ(renditions[1].width, 1280u);
    EXPECT_EQ(renditions[1].assignedGuestCount, 1u); // guest-B

    // Aggregate bitrate calculation
    uint32_t aggBitrate = manager.getAggregateBitrateKbps();
    EXPECT_GT(aggBitrate, 0u);
    EXPECT_EQ(aggBitrate, renditions[0].bitrateKbps + renditions[1].bitrateKbps);
}
