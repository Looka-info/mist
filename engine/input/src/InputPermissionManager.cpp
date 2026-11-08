#include "mist/input/InputPermissionManager.h"

namespace mist::input {

InputPermissionManager::InputPermissionManager(std::string authorizedSessionId) noexcept
    : authorizedSessionId_(std::move(authorizedSessionId)) {}

void InputPermissionManager::setAuthorizedSessionId(std::string sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    authorizedSessionId_ = std::move(sessionId);
}

void InputPermissionManager::requestPermission(uint8_t requestedFeatures) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == PermissionStatus::Disabled) {
        status_ = PermissionStatus::Requested;
        grantedFeatures_ = requestedFeatures;
    }
}

void InputPermissionManager::grantPermission(uint8_t grantedFeatures) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = PermissionStatus::Granted;
    grantedFeatures_ = grantedFeatures;
    localOverrideActive_ = false;
}

void InputPermissionManager::revokePermission() {
    StateReleaseCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ == PermissionStatus::Granted) {
            status_ = PermissionStatus::Revoked;
            grantedFeatures_ = 0;
            cb = releaseCb_;
        }
    }
    if (cb) {
        cb();
    }
}

void InputPermissionManager::setLocalOverride(bool overrideActive) {
    StateReleaseCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        localOverrideActive_ = overrideActive;
        if (overrideActive && status_ == PermissionStatus::Granted) {
            cb = releaseCb_;
        }
    }
    if (cb) {
        cb();
    }
}

bool InputPermissionManager::isInputAllowed(InputType type, const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Always allow permission messages and reset messages through for handshake/recovery
    if (type == InputType::PermissionRequest ||
        type == InputType::PermissionGrant ||
        type == InputType::PermissionRevoke ||
        type == InputType::ResetState) {
        return true;
    }

    if (status_ != PermissionStatus::Granted) {
        return false;
    }

    if (localOverrideActive_) {
        return false;
    }

    if (!authorizedSessionId_.empty() && authorizedSessionId_ != sessionId) {
        return false;
    }

    // Feature mask check: Bit 0: Keyboard, Bit 1: Mouse, Bit 2: Gamepad
    switch (type) {
        case InputType::KeyEvent:
            return (grantedFeatures_ & 0x01) != 0;
        case InputType::MouseMoveEvent:
        case InputType::MouseButtonEvent:
        case InputType::MouseWheelEvent:
            return (grantedFeatures_ & 0x02) != 0;
        case InputType::GamepadState:
            return (grantedFeatures_ & 0x04) != 0;
        default:
            return true;
    }
}

PermissionStatus InputPermissionManager::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

uint8_t InputPermissionManager::getGrantedFeatures() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return grantedFeatures_;
}

bool InputPermissionManager::isLocalOverrideActive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return localOverrideActive_;
}

bool InputPermissionManager::checkRateLimit() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - rateWindowStart_).count();

    if (elapsed >= 1000) {
        rateWindowStart_ = now;
        eventsInWindow_ = 1;
        return true;
    }

    eventsInWindow_++;
    return eventsInWindow_ <= MAX_EVENTS_PER_SECOND;
}

} // namespace mist::input