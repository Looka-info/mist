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

    // Feature mask check: Bit 0: Keyboard, Bit 1: Mouse, Bit 2: Gamepad, Bit 4: Clipboard
    switch (type) {
        case InputType::KeyEvent:
            return (grantedFeatures_ & 0x01) != 0;
        case InputType::MouseMoveEvent:
        case InputType::MouseButtonEvent:
        case InputType::MouseWheelEvent:
            return (grantedFeatures_ & 0x02) != 0;
        case InputType::GamepadState:
            return (grantedFeatures_ & 0x04) != 0;
        case InputType::ClipboardEvent:
            return (grantedFeatures_ & 0x10) != 0;
        default:
            return true;
    }
}

void InputPermissionManager::setGuestPermission(const std::string& guestId, uint8_t features) {
    std::lock_guard<std::mutex> lock(mutex_);
    guestPermissions_[guestId] = GuestPermState{features, false};
    logAuditUnderLock(guestId, InputType::Unknown, true, "Set guest permission features: " + std::to_string(features));
}

void InputPermissionManager::revokeGuest(const std::string& guestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guestPermissions_.find(guestId);
    if (it != guestPermissions_.end()) {
        it->second.features = 0;
        it->second.isRevoked = true;
    } else {
        guestPermissions_[guestId] = GuestPermState{0, true};
    }
    logAuditUnderLock(guestId, InputType::Unknown, true, "Revoked guest input permissions");
}

bool InputPermissionManager::isGuestInputAllowed(const std::string& guestId, InputType type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Always allow permission messages and reset messages through
    if (type == InputType::PermissionRequest ||
        type == InputType::PermissionGrant ||
        type == InputType::PermissionRevoke ||
        type == InputType::ResetState) {
        return true;
    }

    if (localOverrideActive_) {
        logAuditUnderLock(guestId, type, false, "Local override active");
        return false;
    }

    auto it = guestPermissions_.find(guestId);
    if (it == guestPermissions_.end()) {
        logAuditUnderLock(guestId, type, false, "No permission record for guest");
        return false;
    }

    if (it->second.isRevoked) {
        logAuditUnderLock(guestId, type, false, "Guest permissions revoked");
        return false;
    }

    uint8_t perms = it->second.features;
    bool allowed = false;

    switch (type) {
        case InputType::KeyEvent:
            allowed = (perms & FEATURE_KEYBOARD) != 0;
            break;
        case InputType::MouseMoveEvent:
        case InputType::MouseButtonEvent:
        case InputType::MouseWheelEvent:
            allowed = (perms & FEATURE_MOUSE) != 0;
            break;
        case InputType::GamepadState:
            allowed = (perms & FEATURE_GAMEPAD) != 0;
            break;
        case InputType::ClipboardEvent:
            allowed = (perms & FEATURE_CLIPBOARD) != 0;
            break;
        default:
            allowed = false;
            break;
    }

    if (!allowed) {
        logAuditUnderLock(guestId, type, false, "Feature bit not granted");
    }

    return allowed;
}

uint8_t InputPermissionManager::getGuestGrantedFeatures(const std::string& guestId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guestPermissions_.find(guestId);
    if (it != guestPermissions_.end() && !it->second.isRevoked) {
        return it->second.features;
    }
    return 0;
}

std::vector<PermissionAuditRecord> InputPermissionManager::getAuditLog() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return auditLog_;
}

void InputPermissionManager::clearAuditLog() {
    std::lock_guard<std::mutex> lock(mutex_);
    auditLog_.clear();
}

void InputPermissionManager::logAuditUnderLock(const std::string& guestId, InputType type, bool allowed, const std::string& detail) const {
    uint64_t nowNs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    auditLog_.push_back(PermissionAuditRecord{nowNs, guestId, type, allowed, detail});
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