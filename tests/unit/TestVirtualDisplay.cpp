#include <gtest/gtest.h>
#include "mist/platform/windows/VirtualDisplayManager.h"

using namespace mist;
using namespace mist::platform::windows;

TEST(VirtualDisplayTest, Edid14BlockGenerationAndChecksum) {
    // 1080p @ 60Hz
    auto edid1080p = VirtualDisplayManager::generateEdid14(1920, 1080, 60, "Mist 1080p");
    ASSERT_EQ(edid1080p.size(), 128u);

    // VESA Header
    EXPECT_EQ(edid1080p[0], 0x00);
    EXPECT_EQ(edid1080p[1], 0xFF);
    EXPECT_EQ(edid1080p[7], 0x00);

    // Manufacturer ID: "MST"
    EXPECT_EQ(edid1080p[8], 0x36);
    EXPECT_EQ(edid1080p[9], 0x74);

    // Version 1.4
    EXPECT_EQ(edid1080p[18], 0x01);
    EXPECT_EQ(edid1080p[19], 0x04);

    // Checksum verification
    EXPECT_TRUE(VirtualDisplayManager::validateEdidChecksum(edid1080p));

    // 1440p @ 144Hz
    auto edid1440p = VirtualDisplayManager::generateEdid14(2560, 1440, 144, "Mist 1440p");
    ASSERT_EQ(edid1440p.size(), 128u);
    EXPECT_TRUE(VirtualDisplayManager::validateEdidChecksum(edid1440p));

    // 4K @ 120Hz
    auto edid4k = VirtualDisplayManager::generateEdid14(3840, 2160, 120, "Mist 4K");
    ASSERT_EQ(edid4k.size(), 128u);
    EXPECT_TRUE(VirtualDisplayManager::validateEdidChecksum(edid4k));
}

TEST(VirtualDisplayTest, PlugAndUnplugLifecycle) {
    VirtualDisplayConfig config{};
    config.simulationOnly = true;

    VirtualDisplayManager manager(config);
    EXPECT_TRUE(manager.getActiveVirtualDisplays().empty());

    // 1. Plug 1080p display
    VirtualDisplayMode mode1{1920, 1080, 60};
    auto plugRes1 = manager.plugVirtualDisplay(mode1);
    ASSERT_TRUE(plugRes1.hasValue());
    uint32_t mon1 = plugRes1.value();
    EXPECT_GT(mon1, 0u);

    // 2. Plug 1440p display
    VirtualDisplayMode mode2{2560, 1440, 120};
    auto plugRes2 = manager.plugVirtualDisplay(mode2);
    ASSERT_TRUE(plugRes2.hasValue());
    uint32_t mon2 = plugRes2.value();
    EXPECT_NE(mon1, mon2);

    EXPECT_EQ(manager.getActiveVirtualDisplays().size(), 2u);

    // Inspect monitor 1
    auto info1 = manager.getVirtualDisplay(mon1);
    ASSERT_TRUE(info1.hasValue());
    EXPECT_EQ(info1.value().currentMode.width, 1920u);
    EXPECT_EQ(info1.value().currentMode.height, 1080u);
    EXPECT_EQ(info1.value().currentMode.refreshRate, 60u);
    EXPECT_TRUE(info1.value().isPlugged);
    EXPECT_EQ(info1.value().edidBlock.size(), 128u);

    // 3. Unplug monitor 1
    auto unRes1 = manager.unplugVirtualDisplay(mon1);
    EXPECT_TRUE(unRes1.hasValue());
    EXPECT_EQ(manager.getActiveVirtualDisplays().size(), 1u);

    // 4. Unplug all
    auto unAll = manager.unplugAllVirtualDisplays();
    EXPECT_TRUE(unAll.hasValue());
    EXPECT_TRUE(manager.getActiveVirtualDisplays().empty());
}

TEST(VirtualDisplayTest, InvalidModesAndNotFoundRejected) {
    VirtualDisplayConfig config{};
    config.simulationOnly = true;

    VirtualDisplayManager manager(config);

    // Zero width/height/refresh
    VirtualDisplayMode invalidMode{0, 1080, 60};
    auto fail1 = manager.plugVirtualDisplay(invalidMode);
    EXPECT_FALSE(fail1.hasValue());
    EXPECT_EQ(fail1.error().code(), common::ErrorCode::InvalidArgument);

    // Unplug nonexistent
    auto fail2 = manager.unplugVirtualDisplay(999);
    EXPECT_FALSE(fail2.hasValue());
    EXPECT_EQ(fail2.error().code(), common::ErrorCode::NotFound);

    // Query nonexistent
    auto fail3 = manager.getVirtualDisplay(999);
    EXPECT_FALSE(fail3.hasValue());
    EXPECT_EQ(fail3.error().code(), common::ErrorCode::NotFound);
}
