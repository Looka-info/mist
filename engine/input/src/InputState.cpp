#include "mist/input/InputState.h"

namespace mist::input {

void InputState::pressKey(uint32_t scanCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    heldScanCodes_.insert(scanCode);
}

void InputState::releaseKey(uint32_t scanCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    heldScanCodes_.erase(scanCode);
}

bool InputState::isKeyPressed(uint32_t scanCode) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heldScanCodes_.find(scanCode) != heldScanCodes_.end();
}

std::vector<uint32_t> InputState::getHeldScanCodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<uint32_t>(heldScanCodes_.begin(), heldScanCodes_.end());
}

void InputState::setMouseButton(MouseButton button, bool isDown) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(button));
    if (isDown) {
        heldMouseButtonsMask_ |= bit;
    } else {
        heldMouseButtonsMask_ &= static_cast<uint8_t>(~bit);
    }
}

bool InputState::isMouseButtonDown(MouseButton button) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(button));
    return (heldMouseButtonsMask_ & bit) != 0;
}

uint8_t InputState::getHeldMouseButtonMask() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heldMouseButtonsMask_;
}

void InputState::setMousePosition(float normX, float normY) {
    std::lock_guard<std::mutex> lock(mutex_);
    mouseNormalizedX_ = normX;
    mouseNormalizedY_ = normY;
}

void InputState::getMousePosition(float& outNormX, float& outNormY) const {
    std::lock_guard<std::mutex> lock(mutex_);
    outNormX = mouseNormalizedX_;
    outNormY = mouseNormalizedY_;
}

void InputState::updateGamepad(const GamepadState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    gamepadState_ = state;
}

GamepadState InputState::getGamepadState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gamepadState_;
}

void InputState::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    heldScanCodes_.clear();
    heldMouseButtonsMask_ = 0;
    mouseNormalizedX_ = 0.0f;
    mouseNormalizedY_ = 0.0f;
    gamepadState_ = GamepadState{};
}

InputStateSnapshot InputState::getSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    InputStateSnapshot snap;
    snap.heldScanCodes.assign(heldScanCodes_.begin(), heldScanCodes_.end());
    snap.heldMouseButtons = heldMouseButtonsMask_;
    snap.mouseNormalizedX = mouseNormalizedX_;
    snap.mouseNormalizedY = mouseNormalizedY_;
    snap.gamepad = gamepadState_;
    return snap;
}

} // namespace mist::input