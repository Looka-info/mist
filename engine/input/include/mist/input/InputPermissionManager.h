#pragma once

#include "mist/input/InputTypes.h"
#include <string>
#include <mutex>
#include <functional>
#include <chrono>
#include <vector>
#include <unordered_map>

namespace mist::input {

enum class PermissionStatus : uint8_t {
    Disabled = 0,
    Requested = 1,
    Granted = 2,
    Revoked = 3
};

[[nodiscard]] constexpr std::string_view permissionStatusToString(PermissionStatus s) noexcept {
    switch (s) {
        case PermissionStatus::Disabled:  return "Disabled";
        case PermissionStatus::Requested: return "Requested";
        case PermissionStatus::Granted:   return "Granted";
        case PermissionStatus::Revoked:   return "Revoked";
    }
    return "Unknown";
}

using StateReleaseCallback = std::function<void()>;

struct PermissionAuditRecord {
    uint64_t timestampNs{0};
    std::string guestId;
    InputType type{InputType::Unknown};
    bool allowed{false};
    std::string detail;
};

class InputPermissionManager {
public:
    static constexpr uint8_t FEATURE_VIEW_ONLY = 0x00;
    static constexpr uint8_t FEATURE_KEYBOARD  = 0x01;
    static constexpr uint8_t FEATURE_MOUSE     = 0x02;
    static constexpr uint8_t FEATURE_GAMEPAD   = 0x04;
    static constexpr uint8_t FEATURE_AUDIO     = 0x08;
    static constexpr uint8_t FEATURE_CLIPBOARD = 0x10;

    explicit InputPermissionManager(std::string authorizedSessionId = "") noexcept;

    void setAuthorizedSessionId(std::string sessionId);
    [[nodiscard]] const std::string& getAuthorizedSessionId() const noexcept { return authorizedSessionId_; }

    void requestPermission(uint8_t requestedFeatures);
    void grantPermission(uint8_t grantedFeatures = 0x07);
    void revokePermission();
    void setLocalOverride(bool overrideActive);

    // Legacy / single-session check
    [[nodiscard]] bool isInputAllowed(InputType type, const std::string& sessionId) const;
    [[nodiscard]] PermissionStatus getStatus() const;
    [[nodiscard]] uint8_t getGrantedFeatures() const;
    [[nodiscard]] bool isLocalOverrideActive() const;

    // Per-guest permissions API
    void setGuestPermission(const std::string& guestId, uint8_t features);
    void revokeGuest(const std::string& guestId);
    [[nodiscard]] bool isGuestInputAllowed(const std::string& guestId, InputType type);
    [[nodiscard]] uint8_t getGuestGrantedFeatures(const std::string& guestId) const;

    // Permission audit log
    [[nodiscard]] std::vector<PermissionAuditRecord> getAuditLog() const;
    void clearAuditLog();

    void setStateReleaseCallback(StateReleaseCallback cb) { releaseCb_ = std::move(cb); }

    // Rate limiting: returns true if under limit, false if flooded
    [[nodiscard]] bool checkRateLimit();

private:
    void triggerReleaseUnderLock();
    void logAuditUnderLock(const std::string& guestId, InputType type, bool allowed, const std::string& detail) const;

    mutable std::mutex mutex_;
    std::string authorizedSessionId_;
    PermissionStatus status_{PermissionStatus::Disabled};
    uint8_t grantedFeatures_{0};
    bool localOverrideActive_{false};
    StateReleaseCallback releaseCb_;

    // Per-guest permission tracking
    struct GuestPermState {
        uint8_t features{0};
        bool isRevoked{false};
    };
    std::unordered_map<std::string, GuestPermState> guestPermissions_;
    mutable std::vector<PermissionAuditRecord> auditLog_;

    // Rate limiter (max 2,000 events / sec)
    std::chrono::steady_clock::time_point rateWindowStart_{std::chrono::steady_clock::now()};
    uint32_t eventsInWindow_{0};
    static constexpr uint32_t MAX_EVENTS_PER_SECOND = 2000;
};

} // namespace mist::input