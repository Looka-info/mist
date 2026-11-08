#include <gtest/gtest.h>
#include "mist/input/InputState.h"

using namespace mist::input;

class InputStateTest : public ::testing::Test {};

TEST_F(InputStateTest, KeyHoldingTracking) {
    InputState state;
    EXPECT_FALSE(state.isKeyPressed(0x1E));

    state.pressKey(0x1E);
    state.pressKey(0x30);
    EXPECT_TRUE(state.isKeyPressed(0x1E));
    EXPECT_TRUE(state.isKeyPressed(0x30));

    auto held = state.getHeldScanCodes();
    EXPECT_EQ(held.size(), 2u);

    state.releaseKey(0x1E);
    EXPECT_FALSE(state.isKeyPressed(0x1E));
    EXPECT_TRUE(state.isKeyPressed(0x30));

    state.reset();
    EXPECT_FALSE(state.isKeyPressed(0x30));
    EXPECT_EQ(state.getHeldScanCodes().size(), 0u);
}

TEST_F(InputStateTest, MouseButtonsMask) {
    InputState state;
    EXPECT_EQ(state.getHeldMouseButtonMask(), 0u);

    state.setMouseButton(MouseButton::Left, true);
    EXPECT_TRUE(state.isMouseButtonDown(MouseButton::Left));
    EXPECT_FALSE(state.isMouseButtonDown(MouseButton::Right));

    state.setMouseButton(MouseButton::Right, true);
    EXPECT_TRUE(state.isMouseButtonDown(MouseButton::Right));

    state.setMouseButton(MouseButton::Left, false);
    EXPECT_FALSE(state.isMouseButtonDown(MouseButton::Left));
    EXPECT_TRUE(state.isMouseButtonDown(MouseButton::Right));

    state.reset();
    EXPECT_EQ(state.getHeldMouseButtonMask(), 0u);
}

TEST_F(InputStateTest, MousePositionAndSnapshot) {
    InputState state;
    state.setMousePosition(0.35f, 0.75f);

    float x = 0.0f, y = 0.0f;
    state.getMousePosition(x, y);
    EXPECT_NEAR(x, 0.35f, 0.001f);
    EXPECT_NEAR(y, 0.75f, 0.001f);

    state.pressKey(0x11);
    state.setMouseButton(MouseButton::Middle, true);

    auto snap = state.getSnapshot();
    EXPECT_EQ(snap.heldScanCodes.size(), 1u);
    EXPECT_EQ(snap.heldScanCodes[0], 0x11u);
    EXPECT_TRUE(snap.heldMouseButtons & (1u << static_cast<uint8_t>(MouseButton::Middle)));
    EXPECT_NEAR(snap.mouseNormalizedX, 0.35f, 0.001f);
    EXPECT_NEAR(snap.mouseNormalizedY, 0.75f, 0.001f);
}
