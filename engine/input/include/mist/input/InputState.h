#pragma once

#include "mist/input/InputTypes.h"
#include <unordered_set>
#include <vector>
#include <mutex>

namespace mist::input {

struct InputStateSnapshot {
    std::vector<uint32_t> heldScanCodes;
    uint8_t heldMouseButtons{0};
    float mouseNormalizedX{0.0f};
    float mouseNormalizedY{0.0f};
    GamepadState gamepad{};
};

class InputState {
public:
    InputState() = default;

    void pressKey(uint32_t scanCode);
    void releaseKey(uint32_t scanCode);
    [[nodiscard]] bool isKeyPressed(uint32_t scanCode) const;
    [[nodiscard]] std::vector<uint32_t> getHeldScanCodes() const;

    void setMouseButton(MouseButton button, bool isDown);
    [[nodiscard]] bool isMouseButtonDown(MouseButton button) const;
    [[nodiscard]] uint8_t getHeldMouseButtonMask() const;

    void setMousePosition(float normX, float normY);
    void getMousePosition(float& outNormX, float& outNormY) const;

    void updateGamepad(const GamepadState& state);
    [[nodiscard]] GamepadState getGamepadState() const;

    void reset();
    [[nodiscard]] InputStateSnapshot getSnapshot() const;

private:
    mutable std::mutex mutex_;
    std::unordered_set<uint32_t> heldScanCodes_;
    uint8_t heldMouseButtonsMask_{0};
    float mouseNormalizedX_{0.0f};
    float mouseNormalizedY_{0.0f};
    GamepadState gamepadState_{};
};

} // namespace mist::input