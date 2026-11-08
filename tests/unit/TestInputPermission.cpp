#include <gtest/gtest.h>
#include "mist/input/InputPermissionManager.h"

using namespace mist::input;

class InputPermissionTest : public ::testing::Test {};

TEST_F(InputPermissionTest, DefaultDisabledAndGrant) {
    InputPermissionManager pm("session-1");
    EXPECT_EQ(pm.getStatus(), PermissionStatus::Disabled);

    // Any input event must be denied initially
    EXPECT_FALSE(pm.isInputAllowed(InputType::KeyEvent, "session-1"));
    EXPECT_FALSE(pm.isInputAllowed(InputType::MouseMoveEvent, "session-1"));
    EXPECT_FALSE(pm.isInputAllowed(InputType::GamepadState, "session-1"));

    // Handshake messages are always allowed through
    EXPECT_TRUE(pm.isInputAllowed(InputType::PermissionRequest, "session-1"));
    EXPECT_TRUE(pm.isInputAllowed(InputType::ResetState, "session-1"));

    // Explicit grant
    pm.grantPermission(0x07);
    EXPECT_EQ(pm.getStatus(), PermissionStatus::Granted);
    EXPECT_TRUE(pm.isInputAllowed(InputType::KeyEvent, "session-1"));
    EXPECT_TRUE(pm.isInputAllowed(InputType::MouseMoveEvent, "session-1"));
    EXPECT_TRUE(pm.isInputAllowed(InputType::GamepadState, "session-1"));

    // Mismatched session ID denied
    EXPECT_FALSE(pm.isInputAllowed(InputType::KeyEvent, "wrong-session"));
}

TEST_F(InputPermissionTest, FeatureMasking) {
    InputPermissionManager pm("session-1");

    // Only allow keyboard (bit 0 = 0x01)
    pm.grantPermission(0x01);
    EXPECT_TRUE(pm.isInputAllowed(InputType::KeyEvent, "session-1"));
    EXPECT_FALSE(pm.isInputAllowed(InputType::MouseMoveEvent, "session-1"));
    EXPECT_FALSE(pm.isInputAllowed(InputType::GamepadState, "session-1"));

    // Only allow mouse (bit 1 = 0x02)
    pm.grantPermission(0x02);
    EXPECT_FALSE(pm.isInputAllowed(InputType::KeyEvent, "session-1"));
    EXPECT_TRUE(pm.isInputAllowed(InputType::MouseMoveEvent, "session-1"));
    EXPECT_FALSE(pm.isInputAllowed(InputType::GamepadState, "session-1"));
}

TEST_F(InputPermissionTest, LocalOverrideAndRevocation) {
    InputPermissionManager pm("session-1");
    pm.grantPermission(0x07);

    bool releaseTriggered = false;
    pm.setStateReleaseCallback([&]() {
        releaseTriggered = true;
    });

    // Local host override activates
    pm.setLocalOverride(true);
    EXPECT_TRUE(pm.isLocalOverrideActive());
    EXPECT_FALSE(pm.isInputAllowed(InputType::KeyEvent, "session-1"));
    EXPECT_TRUE(releaseTriggered);

    // Release override
    releaseTriggered = false;
    pm.setLocalOverride(false);
    EXPECT_TRUE(pm.isInputAllowed(InputType::KeyEvent, "session-1"));

    // Host revokes permission
    pm.revokePermission();
    EXPECT_EQ(pm.getStatus(), PermissionStatus::Revoked);
    EXPECT_FALSE(pm.isInputAllowed(InputType::KeyEvent, "session-1"));
    EXPECT_TRUE(releaseTriggered);
}

TEST_F(InputPermissionTest, RateLimiter) {
    InputPermissionManager pm("session-1");
    // Send 2000 events (allowed)
    for (int i = 0; i < 2000; ++i) {
        EXPECT_TRUE(pm.checkRateLimit());
    }
    // 2001st event within 1 sec is rejected
    EXPECT_FALSE(pm.checkRateLimit());
}
