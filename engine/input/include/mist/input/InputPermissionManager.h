#pragma once

#include "mist/input/InputTypes.h"
#include <string>
#include <mutex>
#include <functional>
#include <chrono>

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

class InputPermissionManager {
public:
    explicit InputPermissionManager(std::string authorizedSessionId = "") noexcept;

    void setAuthorizedSessionId(std::string sessionId);
    [[nodiscard]] const std::string& getAuthorizedSessionId() const noexcept { return authorizedSessionId_; }

    void requestPermission(uint8_t requestedFeatures);
    void grantPermission(uint8_t grantedFeatures = 0x07);
    void revokePermission();
    void setLocalOverride(bool overrideActive);

    [[nodiscard]] bool isInputAllowed(InputType type, const std::string& sessionId) const;
    [[nodiscard]] PermissionStatus getStatus() const;
    [[nodiscard]] uint8_t getGrantedFeatures() const;
    [[nodiscard]] bool isLocalOverrideActive() const;

    void setStateReleaseCallback(StateReleaseCallback cb) { releaseCb_ = std::move(cb); }

    // Rate limiting: returns true if under limit, false if flooded
    [[nodiscard]] bool checkRateLimit();

private:
    void triggerReleaseUnderLock();

    mutable std::mutex mutex_;
    std::string authorizedSessionId_;
    PermissionStatus status_{PermissionStatus::Disabled};
    uint8_t grantedFeatures_{0};
    bool localOverrideActive_{false};
    StateReleaseCallback releaseCb_;

    // Rate limiter (max 2,000 events / sec)
    std::chrono::steady_clock::time_point rateWindowStart_{std::chrono::steady_clock::now()};
    uint32_t eventsInWindow_{0};
    static constexpr uint32_t MAX_EVENTS_PER_SECOND = 2000;
};

} // namespace mist::input