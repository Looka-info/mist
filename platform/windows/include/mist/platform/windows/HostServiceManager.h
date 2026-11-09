#pragma once

#include "mist/common/Result.h"
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <cstdint>

namespace mist::platform::windows {

enum class ServiceStatus : uint8_t {
    Stopped = 0,
    StartPending,
    StopPending,
    Running,
    ContinuePending,
    PausePending,
    Paused
};

[[nodiscard]] constexpr std::string_view serviceStatusToString(ServiceStatus s) noexcept {
    switch (s) {
        case ServiceStatus::Stopped:         return "Stopped";
        case ServiceStatus::StartPending:    return "StartPending";
        case ServiceStatus::StopPending:     return "StopPending";
        case ServiceStatus::Running:         return "Running";
        case ServiceStatus::ContinuePending: return "ContinuePending";
        case ServiceStatus::PausePending:    return "PausePending";
        case ServiceStatus::Paused:          return "Paused";
    }
    return "Unknown";
}

struct HostServiceConfig {
    std::string serviceName{"MistHostService"};
    std::string displayName{"Mist Game Streaming Host Service"};
    std::string executablePath;
    bool simulationOnly{false};
};

enum class TrayIconState : uint8_t {
    Offline = 0,
    Ready,
    Streaming,
    Paused
};

struct TrayNotification {
    std::string title;
    std::string message;
    uint32_t timeoutMs{3000};
};

class HostServiceManager {
public:
    explicit HostServiceManager(HostServiceConfig config = {}) noexcept;
    ~HostServiceManager() = default;

    // Service lifecycle
    [[nodiscard]] common::Result<void> installService();
    [[nodiscard]] common::Result<void> uninstallService();
    [[nodiscard]] common::Result<void> startService();
    [[nodiscard]] common::Result<void> stopService();
    [[nodiscard]] common::Result<ServiceStatus> queryStatus() const;

    // Tray Companion state
    void setTrayState(TrayIconState state) noexcept;
    [[nodiscard]] TrayIconState getTrayState() const noexcept;
    [[nodiscard]] uint32_t getActiveGuestCount() const noexcept;
    void setActiveGuestCount(uint32_t count) noexcept;

    // Post notification to tray
    void postNotification(TrayNotification notification);
    [[nodiscard]] std::vector<TrayNotification> getNotificationHistory() const;

    [[nodiscard]] const HostServiceConfig& getConfig() const noexcept { return config_; }

private:
    mutable std::mutex mutex_;
    HostServiceConfig config_;
    ServiceStatus simulatedStatus_{ServiceStatus::Stopped};
    TrayIconState trayState_{TrayIconState::Ready};
    uint32_t activeGuests_{0};
    std::vector<TrayNotification> notifications_;
};

} // namespace mist::platform::windows
