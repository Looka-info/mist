#include <gtest/gtest.h>
#include "mist/input/MultiGuestGamepadManager.h"

using namespace mist::input;

TEST(MultiGuestGamepadTest, SlotAssignmentAndCapacityLimits) {
    MultiGuestGamepadManager manager;
    ASSERT_TRUE(manager.initialize().hasValue());

    // Assign up to 4 guests
    auto s0 = manager.assignGuest("guest-1");
    ASSERT_TRUE(s0.hasValue());
    EXPECT_EQ(s0.value(), 0u);

    auto s1 = manager.assignGuest("guest-2");
    ASSERT_TRUE(s1.hasValue());
    EXPECT_EQ(s1.value(), 1u);

    auto s2 = manager.assignGuest("guest-3");
    ASSERT_TRUE(s2.hasValue());
    EXPECT_EQ(s2.value(), 2u);

    auto s3 = manager.assignGuest("guest-4");
    ASSERT_TRUE(s3.hasValue());
    EXPECT_EQ(s3.value(), 3u);

    EXPECT_EQ(manager.getActiveSlotCount(), 4u);

    // Idempotent re-assignment
    auto s1_again = manager.assignGuest("guest-2");
    ASSERT_TRUE(s1_again.hasValue());
    EXPECT_EQ(s1_again.value(), 1u);

    // 5th guest should be rejected (all 4 slots occupied)
    auto s4 = manager.assignGuest("guest-5");
    EXPECT_FALSE(s4.hasValue());
    EXPECT_EQ(s4.error().code(), mist::common::ErrorCode::ResourceExhausted);

    // Release guest-2
    manager.releaseGuest("guest-2");
    EXPECT_EQ(manager.getActiveSlotCount(), 3u);
    EXPECT_FALSE(manager.getGuestSlot("guest-2").has_value());

    // Slot 1 is now available for new guest-5
    auto s5 = manager.assignGuest("guest-5");
    ASSERT_TRUE(s5.hasValue());
    EXPECT_EQ(s5.value(), 1u);
    EXPECT_EQ(manager.getActiveSlotCount(), 4u);
}

TEST(MultiGuestGamepadTest, InputFeedingAndDeadZones) {
    MultiGuestGamepadManager manager;
    ASSERT_TRUE(manager.initialize().hasValue());

    auto s0 = manager.assignGuest("guest-alice");
    ASSERT_TRUE(s0.hasValue());

    // Create raw state with stick inside deadzone (< 7849)
    GamepadState rawState{};
    rawState.buttons = 0x1000; // A button
    rawState.thumbLX = 5000;   // Inside deadzone
    rawState.thumbLY = -20000; // Beyond deadzone
    rawState.leftTrigger = 200;

    auto feedRes = manager.feedGuestGamepadState("guest-alice", rawState, true);
    EXPECT_TRUE(feedRes.hasValue());

    auto slots = manager.getSlotsSnapshot();
    const auto& slot0 = slots[0];
    EXPECT_EQ(slot0.statesReceived, 1u);
    EXPECT_EQ(slot0.lastState.buttons, 0x1000);
    EXPECT_EQ(slot0.lastState.thumbLX, 0); // Filtered by deadzone!
    EXPECT_EQ(slot0.lastState.thumbLY, -20000);
    EXPECT_EQ(slot0.lastState.leftTrigger, 200);

    // Feeding unassigned guest fails
    auto unassignedRes = manager.feedGuestGamepadState("guest-unknown", rawState, true);
    EXPECT_FALSE(unassignedRes.hasValue());
    EXPECT_EQ(unassignedRes.error().code(), mist::common::ErrorCode::NotFound);

    // Permission denied if gamepad permission is revoked
    auto permDeniedRes = manager.feedGuestGamepadState("guest-alice", rawState, false);
    EXPECT_FALSE(permDeniedRes.hasValue());
    EXPECT_EQ(permDeniedRes.error().code(), mist::common::ErrorCode::PermissionDenied);
}

TEST(MultiGuestGamepadTest, IndependentRumbleBackchannel) {
    MultiGuestGamepadManager manager;
    ASSERT_TRUE(manager.initialize().hasValue());

    ASSERT_TRUE(manager.assignGuest("player-1").hasValue());
    ASSERT_TRUE(manager.assignGuest("player-2").hasValue());

    std::string notifiedGuest;
    GamepadVibration notifiedVib{};

    manager.setRumbleCallback([&](const std::string& guestId, const GamepadVibration& vib) {
        notifiedGuest = guestId;
        notifiedVib = vib;
    });

    // Send rumble to player 2 (slot 1)
    auto res = manager.setSlotVibration(1, 45000, 25000);
    EXPECT_TRUE(res.hasValue());

    EXPECT_EQ(notifiedGuest, "player-2");
    EXPECT_EQ(notifiedVib.slotIndex, 1u);
    EXPECT_EQ(notifiedVib.leftMotorSpeed, 45000u);
    EXPECT_EQ(notifiedVib.rightMotorSpeed, 25000u);

    auto slots = manager.getSlotsSnapshot();
    EXPECT_EQ(slots[1].rumbleDispatched, 1u);
    EXPECT_EQ(slots[0].rumbleDispatched, 0u); // Slot 0 remains 0
}

TEST(MultiGuestGamepadTest, ResetAllCleansUpSlots) {
    MultiGuestGamepadManager manager;
    ASSERT_TRUE(manager.initialize().hasValue());

    ASSERT_TRUE(manager.assignGuest("g1").hasValue());
    ASSERT_TRUE(manager.assignGuest("g2").hasValue());
    EXPECT_EQ(manager.getActiveSlotCount(), 2u);

    manager.resetAll();
    EXPECT_EQ(manager.getActiveSlotCount(), 0u);
    EXPECT_FALSE(manager.getGuestSlot("g1").has_value());
    EXPECT_FALSE(manager.getGuestSlot("g2").has_value());
}
