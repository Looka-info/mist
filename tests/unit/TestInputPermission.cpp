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

TEST_F(InputPermissionTest, PerGuestPermissionsAndClipboard) {
    InputPermissionManager pm("session-host");

    // Guest 1: Keyboard and Mouse
    pm.setGuestPermission("guest-km", InputPermissionManager::FEATURE_KEYBOARD | InputPermissionManager::FEATURE_MOUSE);

    // Guest 2: Gamepad only (Player 2)
    pm.setGuestPermission("guest-gamepad", InputPermissionManager::FEATURE_GAMEPAD);

    // Guest 3: Clipboard only
    pm.setGuestPermission("guest-clip", InputPermissionManager::FEATURE_CLIPBOARD);

    // Guest 4: View-Only
    pm.setGuestPermission("guest-view", InputPermissionManager::FEATURE_VIEW_ONLY);

    // Verify Guest 1
    EXPECT_TRUE(pm.isGuestInputAllowed("guest-km", InputType::KeyEvent));
    EXPECT_TRUE(pm.isGuestInputAllowed("guest-km", InputType::MouseMoveEvent));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-km", InputType::GamepadState));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-km", InputType::ClipboardEvent));

    // Verify Guest 2 (Gamepad only)
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-gamepad", InputType::KeyEvent));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-gamepad", InputType::MouseMoveEvent));
    EXPECT_TRUE(pm.isGuestInputAllowed("guest-gamepad", InputType::GamepadState));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-gamepad", InputType::ClipboardEvent));

    // Verify Guest 3 (Clipboard)
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-clip", InputType::KeyEvent));
    EXPECT_TRUE(pm.isGuestInputAllowed("guest-clip", InputType::ClipboardEvent));

    // Verify Guest 4 (View only)
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-view", InputType::KeyEvent));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-view", InputType::MouseMoveEvent));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-view", InputType::GamepadState));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-view", InputType::ClipboardEvent));
}

TEST_F(InputPermissionTest, RuntimeRevocationAndAuditLog) {
    InputPermissionManager pm("session-host");

    pm.setGuestPermission("guest-active", InputPermissionManager::FEATURE_KEYBOARD | InputPermissionManager::FEATURE_MOUSE);
    EXPECT_TRUE(pm.isGuestInputAllowed("guest-active", InputType::KeyEvent));

    // Disallowed event triggers audit log
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-active", InputType::GamepadState));

    // Runtime immediate revocation
    pm.revokeGuest("guest-active");
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-active", InputType::KeyEvent));
    EXPECT_FALSE(pm.isGuestInputAllowed("guest-active", InputType::MouseMoveEvent));

    auto log = pm.getAuditLog();
    EXPECT_GE(log.size(), 3u);
    EXPECT_FALSE(log.empty());

    // Audit record contains details
    bool foundRevoked = false;
    for (const auto& entry : log) {
        if (entry.guestId == "guest-active" && entry.detail.find("Revoked") != std::string::npos) {
            foundRevoked = true;
            break;
        }
    }
    EXPECT_TRUE(foundRevoked);
}
