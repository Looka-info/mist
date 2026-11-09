#include <gtest/gtest.h>
#include "mist/transport/qos/HostBandwidthBudgetManager.h"

using namespace mist;
using namespace mist::transport::qos;

class HostBandwidthBudgetTest : public ::testing::Test {};

TEST_F(HostBandwidthBudgetTest, SingleGuestReservationAndAllocations) {
    // 30 Mbps total upload cap, 5% headroom
    HostBandwidthBudgetManager manager(30000, 5);
    EXPECT_EQ(manager.getUploadLimitKbps(), 30000u);
    EXPECT_EQ(manager.getGuestCount(), 0u);

    // Initial empty budget
    auto initialSnap = manager.getSnapshot();
    EXPECT_EQ(initialSnap.totalAllocatedKbps, 0u);
    EXPECT_EQ(initialSnap.headroomKbps, 1500u); // 5% of 30000
    EXPECT_EQ(initialSnap.unallocatedKbps, 28500u);

    // Add Guest 1 demanding 12 Mbps video
    EXPECT_TRUE(manager.addGuest("guest-1", 12000, true, 128, 64).hasValue());
    EXPECT_EQ(manager.getGuestCount(), 1u);

    auto snap = manager.getSnapshot();
    EXPECT_EQ(snap.guestCount, 1u);
    EXPECT_EQ(snap.totalVideoAllocatedKbps, 12000u);
    EXPECT_EQ(snap.totalAudioReservedKbps, 128u);
    EXPECT_EQ(snap.totalControlReservedKbps, 64u);
    EXPECT_FALSE(snap.isOversubscribed);

    auto g1 = manager.getGuestAllocation("guest-1").value();
    EXPECT_EQ(g1.allocatedVideoKbps, 12000u);
    EXPECT_EQ(g1.reservedAudioKbps, 128u);
    EXPECT_EQ(g1.reservedControlKbps, 64u);
    EXPECT_EQ(g1.totalGuestKbps, 12000u + 128u + 64u);
    EXPECT_FALSE(g1.isThrottled);
}

TEST_F(HostBandwidthBudgetTest, EqualShareFairnessUnderTotalUploadCap) {
    // 20 Mbps total upload cap, 5% headroom (1000 kbps), net available = 19000 kbps
    HostBandwidthBudgetManager manager(20000, 5, 2000, 20000);
    manager.setAllocationPolicy(AllocationPolicy::EqualShare);

    // 3 guests each asking for 12 Mbps video (total video demand = 36 Mbps, exceeds 19 Mbps)
    EXPECT_TRUE(manager.addGuest("guest-A", 12000, true, 128, 64).hasValue());
    EXPECT_TRUE(manager.addGuest("guest-B", 12000, true, 128, 64).hasValue());
    EXPECT_TRUE(manager.addGuest("guest-C", 12000, true, 128, 64).hasValue());

    auto snap = manager.getSnapshot();
    EXPECT_EQ(snap.guestCount, 3u);
    // Audio reserved: 3 * 128 = 384 kbps
    EXPECT_EQ(snap.totalAudioReservedKbps, 384u);
    // Control reserved: 3 * 64 = 192 kbps
    EXPECT_EQ(snap.totalControlReservedKbps, 192u);

    // Video budget = 19000 - 384 - 192 = 18424 kbps
    // Equal share per guest = 18424 / 3 = 6141 kbps
    auto a = manager.getGuestAllocation("guest-A").value();
    auto b = manager.getGuestAllocation("guest-B").value();
    auto c = manager.getGuestAllocation("guest-C").value();

    EXPECT_EQ(a.allocatedVideoKbps, b.allocatedVideoKbps);
    EXPECT_EQ(b.allocatedVideoKbps, c.allocatedVideoKbps);
    EXPECT_TRUE(a.isThrottled);
    EXPECT_TRUE(b.isThrottled);
    EXPECT_TRUE(c.isThrottled);

    // Verify aggregate bandwidth never breaches the host physical upload cap
    EXPECT_LE(snap.totalAllocatedKbps + snap.headroomKbps, 20000u);
    EXPECT_FALSE(snap.isOversubscribed);
}

TEST_F(HostBandwidthBudgetTest, PriorityWeightedActivePlayerAllocation) {
    // 15 Mbps total upload cap
    HostBandwidthBudgetManager manager(15000, 5);
    manager.setAllocationPolicy(AllocationPolicy::PriorityWeighted);

    // Guest Player is active player (weight 2.0), Guest Viewer is spectator (weight 1.0)
    EXPECT_TRUE(manager.addGuest("player-1", 10000, /*isInteractive=*/true).hasValue());
    EXPECT_TRUE(manager.addGuest("viewer-1", 10000, /*isInteractive=*/false).hasValue());

    auto p1 = manager.getGuestAllocation("player-1").value();
    auto v1 = manager.getGuestAllocation("viewer-1").value();

    // Active player receives roughly 2x the video share of the spectator
    EXPECT_GT(p1.allocatedVideoKbps, v1.allocatedVideoKbps);
    double ratio = static_cast<double>(p1.allocatedVideoKbps) / static_cast<double>(v1.allocatedVideoKbps);
    EXPECT_NEAR(ratio, 2.0, 0.15);
}

TEST_F(HostBandwidthBudgetTest, DynamicUploadLimitAdjustment) {
    HostBandwidthBudgetManager manager(30000, 5);
    EXPECT_TRUE(manager.addGuest("guest-1", 15000).hasValue());

    auto allocBefore = manager.getGuestAllocation("guest-1").value();
    EXPECT_EQ(allocBefore.allocatedVideoKbps, 15000u);
    EXPECT_FALSE(allocBefore.isThrottled);

    // Host operator limits upload to 8 Mbps
    manager.setUploadLimitKbps(8000);

    auto allocAfter = manager.getGuestAllocation("guest-1").value();
    EXPECT_LT(allocAfter.allocatedVideoKbps, 15000u);
    EXPECT_TRUE(allocAfter.isThrottled);

    auto snap = manager.getSnapshot();
    EXPECT_LE(snap.totalAllocatedKbps + snap.headroomKbps, 8000u);
}
