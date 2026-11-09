#include <gtest/gtest.h>
#include "mist/input/RelativeMouse.h"
#include "mist/input/ClipboardSync.h"
#include "mist/input/InputProtocol.h"
#include "mist/input/InputPermissionManager.h"

using namespace mist;
using namespace mist::input;

TEST(RelativeMouseTest, ModeTransitionsAndLocking) {
    RelativeMouseConfig config{};
    config.simulationOnly = true;

    RelativeMouseController controller(config);
    EXPECT_FALSE(controller.isLocked());
    EXPECT_EQ(controller.getCaptureMode(), MouseCaptureMode::Disabled);

    int modeChangedCount = 0;
    MouseCaptureMode lastReportedMode = MouseCaptureMode::Disabled;
    controller.setModeChangedCallback([&](MouseCaptureMode mode) {
        modeChangedCount++;
        lastReportedMode = mode;
    });

    auto lockRes = controller.lockCursor(1920, 1080);
    EXPECT_TRUE(lockRes.hasValue());
    EXPECT_TRUE(controller.isLocked());
    EXPECT_EQ(controller.getCaptureMode(), MouseCaptureMode::LockedRelative);
    EXPECT_EQ(modeChangedCount, 1);
    EXPECT_EQ(lastReportedMode, MouseCaptureMode::LockedRelative);

    auto toggleRes = controller.toggleLock(1920, 1080);
    EXPECT_TRUE(toggleRes.hasValue());
    EXPECT_FALSE(controller.isLocked());
    EXPECT_EQ(controller.getCaptureMode(), MouseCaptureMode::Disabled);
    EXPECT_EQ(modeChangedCount, 2);

    auto unlockRes = controller.unlockCursor();
    EXPECT_TRUE(unlockRes.hasValue());
    EXPECT_FALSE(controller.isLocked());
}

TEST(RelativeMouseTest, DpiNormalizationAndSensitivity) {
    RelativeMouseConfig config{};
    config.simulationOnly = true;
    config.sensitivity = 1.0f;
    config.clientDpiScale = 1.0f;
    config.hostDpiScale = 1.0f;

    RelativeMouseController controller(config);

    // 1:1 baseline
    auto ev1 = controller.processRawDelta(10, -20);
    EXPECT_FALSE(ev1.isAbsolute);
    EXPECT_EQ(ev1.deltaX, 10);
    EXPECT_EQ(ev1.deltaY, -20);

    // Sensitivity 2.0x
    controller.setSensitivity(2.0f);
    auto ev2 = controller.processRawDelta(10, -20);
    EXPECT_FALSE(ev2.isAbsolute);
    EXPECT_EQ(ev2.deltaX, 20);
    EXPECT_EQ(ev2.deltaY, -40);

    // Client display is 4K high DPI (2.0 scale), Host display is 1080p (1.0 scale)
    // 2.0x sensitivity with (1.0 / 2.0) DPI ratio = 1.0x effective
    controller.setDpiScales(2.0f, 1.0f);
    auto ev3 = controller.processRawDelta(10, -20);
    EXPECT_FALSE(ev3.isAbsolute);
    EXPECT_EQ(ev3.deltaX, 10);
    EXPECT_EQ(ev3.deltaY, -20);
}

TEST(RelativeMouseTest, UngrabHotkeyDetection) {
    RelativeMouseConfig config{};
    config.simulationOnly = true;
    config.ungrabOnEscape = true;
    config.ungrabOnCtrlAlt = true;

    RelativeMouseController controller(config);
    ASSERT_TRUE(controller.lockCursor(1920, 1080).hasValue());
    EXPECT_TRUE(controller.isLocked());

    // Non-escape key: no unlock
    EXPECT_FALSE(controller.checkUngrabHotkey(0x41 /* 'A' */, false, false));
    EXPECT_TRUE(controller.isLocked());

    // Escape key: triggers unlock
    EXPECT_TRUE(controller.checkUngrabHotkey(0x1B /* VK_ESCAPE */, false, false));
    EXPECT_FALSE(controller.isLocked());

    // Re-lock
    ASSERT_TRUE(controller.lockCursor(1920, 1080).hasValue());
    EXPECT_TRUE(controller.isLocked());

    // Ctrl+Alt key chord: triggers unlock
    EXPECT_TRUE(controller.checkUngrabHotkey(0, true /* ctrl */, true /* alt */));
    EXPECT_FALSE(controller.isLocked());
}

TEST(ClipboardSyncTest, PermissionGatingAndSizeLimits) {
    ClipboardSyncConfig config{};
    config.simulationOnly = true;
    config.maxTextBytes = 1024; // 1 KB test limit

    ClipboardSyncController controller(config);
    InputPermissionManager permManager("session-1");

    std::string guestNoPerm = "guest-unprivileged";
    std::string guestWithPerm = "guest-privileged";

    // Grant mouse & keyboard only, NO clipboard
    permManager.setGuestPermission(guestNoPerm, InputPermissionManager::FEATURE_KEYBOARD | InputPermissionManager::FEATURE_MOUSE);
    // Grant clipboard
    permManager.setGuestPermission(guestWithPerm, InputPermissionManager::FEATURE_KEYBOARD | InputPermissionManager::FEATURE_CLIPBOARD);

    // 1. Unprivileged guest attempt must fail with PermissionDenied
    auto res1 = controller.processIncomingGuestClipboard(guestNoPerm, "Hello from guest", permManager);
    EXPECT_FALSE(res1.hasValue());
    EXPECT_EQ(res1.error().code(), common::ErrorCode::PermissionDenied);

    // 2. Privileged guest attempt succeeds
    auto res2 = controller.processIncomingGuestClipboard(guestWithPerm, "Hello from guest", permManager);
    EXPECT_TRUE(res2.hasValue());
    EXPECT_EQ(controller.getSimulatedClipboardText(), "Hello from guest");

    // 3. Oversized text rejected
    std::string bigText(2048, 'X');
    auto res3 = controller.processIncomingGuestClipboard(guestWithPerm, bigText, permManager);
    EXPECT_FALSE(res3.hasValue());
    EXPECT_EQ(res3.error().code(), common::ErrorCode::InvalidArgument);

    // 4. Binary / null character rejected
    std::string binaryText = "Hello\0World";
    std::string binaryString(binaryText.data(), 11);
    auto res4 = controller.processIncomingGuestClipboard(guestWithPerm, binaryString, permManager);
    EXPECT_FALSE(res4.hasValue());
    EXPECT_EQ(res4.error().code(), common::ErrorCode::InvalidArgument);
}

TEST(ClipboardSyncTest, EchoSuppressionAndHashing) {
    ClipboardSyncConfig config{};
    config.simulationOnly = true;

    ClipboardSyncController controller(config);
    InputPermissionManager permManager("session-1");
    permManager.setGuestPermission("guest-1", InputPermissionManager::FEATURE_CLIPBOARD);

    // Host prepares outgoing text
    std::string textToSync = "https://github.com/Looka-info/mist";
    auto outRes = controller.prepareOutgoingClipboard(textToSync);
    EXPECT_TRUE(outRes.hasValue());
    EXPECT_EQ(outRes.value(), textToSync);
    uint64_t sentHash = controller.getLastSentHash();
    EXPECT_NE(sentHash, 0ULL);

    // Remote client echoes our text back: should be cleanly suppressed
    auto echoRes = controller.processIncomingGuestClipboard("guest-1", textToSync, permManager);
    EXPECT_TRUE(echoRes.hasValue()); // Echo dropped without error

    // Guest sends genuinely new text
    std::string newRemoteText = "New text from guest";
    auto inRes = controller.processIncomingGuestClipboard("guest-1", newRemoteText, permManager);
    EXPECT_TRUE(inRes.hasValue());
    EXPECT_EQ(controller.getSimulatedClipboardText(), newRemoteText);
    uint64_t receivedHash = controller.getLastReceivedHash();
    EXPECT_NE(receivedHash, 0ULL);

    // If local system tries to re-send what was just received from remote, it is suppressed
    auto localOutRes = controller.prepareOutgoingClipboard(newRemoteText);
    EXPECT_FALSE(localOutRes.hasValue());
    EXPECT_EQ(localOutRes.error().code(), common::ErrorCode::AlreadyExists);
}

TEST(ClipboardSyncTest, ProtocolSerializationRoundtrip) {
    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = 42;
    hdr.timestampNs = 1234567890ULL;
    std::strncpy(hdr.sessionId, "test-session", sizeof(hdr.sessionId) - 1);

    std::string originalText = "Test clipboard payload with UTF-8 characters: Привет мир! 🚀";

    auto bytes = InputProtocol::serializeClipboardEvent(hdr, originalText);
    EXPECT_GT(bytes.size(), sizeof(InputPacketHeader));

    auto deserializedRes = InputProtocol::deserialize(bytes);
    ASSERT_TRUE(deserializedRes.hasValue());

    const auto& [outHdr, outEv] = deserializedRes.value();
    EXPECT_EQ(outHdr.sequenceNumber, 42);
    EXPECT_EQ(outHdr.timestampNs, 1234567890ULL);
    EXPECT_STREQ(outHdr.sessionId, "test-session");

    ASSERT_TRUE(std::holds_alternative<ClipboardEvent>(outEv));
    const auto& clipEv = std::get<ClipboardEvent>(outEv);
    EXPECT_EQ(clipEv.text, originalText);
    EXPECT_NE(clipEv.hash, 0ULL);
}
