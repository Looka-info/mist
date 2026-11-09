#include <gtest/gtest.h>
#include "mist/platform/windows/WakeOnLan.h"
#include "mist/platform/windows/HostServiceManager.h"

using namespace mist;
using namespace mist::platform::windows;

TEST(WakeOnLanTest, MacAddressParsingAndFormatting) {
    // 1. Colon separated
    auto res1 = WakeOnLan::parseMacAddress("00:1A:2B:3C:4D:5E");
    ASSERT_TRUE(res1.hasValue());
    MacAddress expected{0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    EXPECT_EQ(res1.value(), expected);
    EXPECT_EQ(WakeOnLan::formatMacAddress(res1.value()), "00:1A:2B:3C:4D:5E");

    // 2. Dash separated
    auto res2 = WakeOnLan::parseMacAddress("00-1a-2b-3c-4d-5e");
    ASSERT_TRUE(res2.hasValue());
    EXPECT_EQ(res2.value(), expected);

    // 3. Compact string
    auto res3 = WakeOnLan::parseMacAddress("001A2B3C4D5E");
    ASSERT_TRUE(res3.hasValue());
    EXPECT_EQ(res3.value(), expected);

    // 4. Invalid hex character rejected
    auto fail1 = WakeOnLan::parseMacAddress("00:1Z:2B:3C:4D:5E");
    EXPECT_FALSE(fail1.hasValue());
    EXPECT_EQ(fail1.error().code(), common::ErrorCode::InvalidArgument);

    // 5. Short MAC string rejected
    auto fail2 = WakeOnLan::parseMacAddress("00:1A:2B:3C");
    EXPECT_FALSE(fail2.hasValue());
    EXPECT_EQ(fail2.error().code(), common::ErrorCode::InvalidArgument);
}

TEST(WakeOnLanTest, MagicPacketStructure) {
    MacAddress mac{0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    auto packet = WakeOnLan::createMagicPacket(mac);

    // Size must be exactly 102 bytes
    ASSERT_EQ(packet.size(), 102u);

    // First 6 bytes: 0xFF
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(packet[i], 0xFF);
    }

    // 16 copies of the 6-byte MAC
    for (size_t rep = 0; rep < 16; ++rep) {
        size_t offset = 6 + (rep * 6);
        for (size_t b = 0; b < 6; ++b) {
            EXPECT_EQ(packet[offset + b], mac[b]);
        }
    }
}

TEST(WakeOnLanTest, SendMagicPacketSimulation) {
    MacAddress mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    auto sendRes = WakeOnLan::sendMagicPacket(mac, 9, "255.255.255.255", /*simulationOnly=*/true);
    EXPECT_TRUE(sendRes.hasValue());
}

TEST(HostServiceTest, LifecycleAndStateTransitions) {
    HostServiceConfig config{};
    config.simulationOnly = true;

    HostServiceManager manager(config);

    // Initial state: Stopped
    auto st0 = manager.queryStatus();
    ASSERT_TRUE(st0.hasValue());
    EXPECT_EQ(st0.value(), ServiceStatus::Stopped);

    // Install
    EXPECT_TRUE(manager.installService().hasValue());

    // Start
    EXPECT_TRUE(manager.startService().hasValue());
    auto st1 = manager.queryStatus();
    ASSERT_TRUE(st1.hasValue());
    EXPECT_EQ(st1.value(), ServiceStatus::Running);

    // Stop
    EXPECT_TRUE(manager.stopService().hasValue());
    auto st2 = manager.queryStatus();
    ASSERT_TRUE(st2.hasValue());
    EXPECT_EQ(st2.value(), ServiceStatus::Stopped);

    // Uninstall
    EXPECT_TRUE(manager.uninstallService().hasValue());
}

TEST(HostServiceTest, TrayCompanionAndNotifications) {
    HostServiceConfig config{};
    config.simulationOnly = true;

    HostServiceManager manager(config);
    EXPECT_EQ(manager.getTrayState(), TrayIconState::Ready);
    EXPECT_EQ(manager.getActiveGuestCount(), 0u);

    // Connect guests
    manager.setActiveGuestCount(2);
    EXPECT_EQ(manager.getActiveGuestCount(), 2u);
    EXPECT_EQ(manager.getTrayState(), TrayIconState::Streaming);

    // Disconnect guests
    manager.setActiveGuestCount(0);
    EXPECT_EQ(manager.getActiveGuestCount(), 0u);
    EXPECT_EQ(manager.getTrayState(), TrayIconState::Ready);

    // Post notifications
    manager.postNotification(TrayNotification{"Guest Connected", "Guest-1 joined session", 3000});
    manager.postNotification(TrayNotification{"Quality Changed", "Stream adapted to 60 FPS", 2000});

    auto history = manager.getNotificationHistory();
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0].title, "Guest Connected");
    EXPECT_EQ(history[1].title, "Quality Changed");
}
