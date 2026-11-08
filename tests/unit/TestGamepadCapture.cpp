#include <gtest/gtest.h>
#include "mist/input/GamepadInput.h"

using namespace mist::input;

class GamepadTest : public ::testing::Test {};

TEST_F(GamepadTest, DeadZoneFiltering) {
    GamepadState raw{};
    raw.thumbLX = 7000;  // Below left deadzone (7849) -> 0
    raw.thumbLY = -7850; // Above left deadzone -> preserved
    raw.thumbRX = 8000;  // Below right deadzone (8689) -> 0
    raw.thumbRY = 12000; // Above right deadzone -> preserved
    raw.leftTrigger = 25;  // Below trigger threshold (30) -> 0
    raw.rightTrigger = 100; // Above trigger threshold -> preserved

    auto filtered = ClientGamepadCapture::applyDeadZones(raw);
    EXPECT_EQ(filtered.thumbLX, 0);
    EXPECT_EQ(filtered.thumbLY, -7850);
    EXPECT_EQ(filtered.thumbRX, 0);
    EXPECT_EQ(filtered.thumbRY, 12000);
    EXPECT_EQ(filtered.leftTrigger, 0u);
    EXPECT_EQ(filtered.rightTrigger, 100u);
}

TEST_F(GamepadTest, HostVirtualGamepadFallback) {
    HostVirtualGamepad vpad(/*simulationOnly=*/true);
    auto initRes = vpad.initialize();
    EXPECT_TRUE(initRes.hasValue());

    EXPECT_FALSE(vpad.getDriverStatus().empty());

    GamepadState state{};
    state.buttons = 0x1000;
    state.thumbLX = 15000;
    EXPECT_TRUE(vpad.updateState(state).hasValue());

    vpad.reset();
    // After reset, state is cleared
}
