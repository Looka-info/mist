#include "mist/platform/windows/HostServiceManager.h"
#include "mist/common/Logger.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsvc.h>
#endif

namespace mist::platform::windows {

HostServiceManager::HostServiceManager(HostServiceConfig config) noexcept
    : config_(std::move(config)) {}

common::Result<void> HostServiceManager::installService() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.simulationOnly) {
        simulatedStatus_ = ServiceStatus::Stopped;
        return common::Result<void>::success();
    }

#if defined(_WIN32)
    SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::PermissionDenied,
                             "Failed to open Service Control Manager (Administrator rights required)");
    }

    std::string binPath = config_.executablePath.empty() ? "mist-host.exe --service" : config_.executablePath;

    SC_HANDLE hService = CreateServiceA(
        hSCM,
        config_.serviceName.c_str(),
        config_.displayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        binPath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!hService) {
        DWORD err = GetLastError();
        CloseServiceHandle(hSCM);
        if (err == ERROR_SERVICE_EXISTS) {
            return common::Result<void>::success();
        }
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError,
                             "CreateService failed: " + std::to_string(err));
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return common::Result<void>::success();
#else
    return common::Result<void>::success();
#endif
}

common::Result<void> HostServiceManager::uninstallService() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.simulationOnly) {
        simulatedStatus_ = ServiceStatus::Stopped;
        return common::Result<void>::success();
    }

#if defined(_WIN32)
    SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::PermissionDenied,
                             "Failed to open SCM to delete service");
    }

    SC_HANDLE hService = OpenServiceA(hSCM, config_.serviceName.c_str(), DELETE);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return common::Result<void>::success(); // Already uninstalled
    }

    BOOL ok = DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    if (!ok) {
        DWORD err = GetLastError();
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError,
                             "DeleteService failed: " + std::to_string(err));
    }

    return common::Result<void>::success();
#else
    return common::Result<void>::success();
#endif
}

common::Result<void> HostServiceManager::startService() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.simulationOnly) {
        simulatedStatus_ = ServiceStatus::Running;
        return common::Result<void>::success();
    }

#if defined(_WIN32)
    SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::PermissionDenied,
                             "Failed to open SCM to start service");
    }

    SC_HANDLE hService = OpenServiceA(hSCM, config_.serviceName.c_str(), SERVICE_START);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NotFound,
                             "Service " + config_.serviceName + " not found");
    }

    BOOL ok = StartServiceA(hService, 0, nullptr);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING) {
            return common::Result<void>::success();
        }
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError,
                             "StartService failed: " + std::to_string(err));
    }

    return common::Result<void>::success();
#else
    simulatedStatus_ = ServiceStatus::Running;
    return common::Result<void>::success();
#endif
}

common::Result<void> HostServiceManager::stopService() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.simulationOnly) {
        simulatedStatus_ = ServiceStatus::Stopped;
        return common::Result<void>::success();
    }

#if defined(_WIN32)
    SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::PermissionDenied,
                             "Failed to open SCM to stop service");
    }

    SC_HANDLE hService = OpenServiceA(hSCM, config_.serviceName.c_str(), SERVICE_STOP);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return common::Result<void>::success();
    }

    SERVICE_STATUS sStatus{};
    BOOL ok = ControlService(hService, SERVICE_CONTROL_STOP, &sStatus);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_NOT_ACTIVE) {
            return common::Result<void>::success();
        }
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError,
                             "ControlService STOP failed: " + std::to_string(err));
    }

    return common::Result<void>::success();
#else
    simulatedStatus_ = ServiceStatus::Stopped;
    return common::Result<void>::success();
#endif
}

common::Result<ServiceStatus> HostServiceManager::queryStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.simulationOnly) {
        return simulatedStatus_;
    }

#if defined(_WIN32)
    SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::PermissionDenied,
                             "Failed to open SCM to query status");
    }

    SC_HANDLE hService = OpenServiceA(hSCM, config_.serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return ServiceStatus::Stopped;
    }

    SERVICE_STATUS_PROCESS ssp{};
    DWORD bytesNeeded = 0;
    BOOL ok = QueryServiceStatusEx(
        hService,
        SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&ssp),
        sizeof(ssp),
        &bytesNeeded
    );

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    if (!ok) {
        return ServiceStatus::Stopped;
    }

    switch (ssp.dwCurrentState) {
        case SERVICE_RUNNING:          return ServiceStatus::Running;
        case SERVICE_STOPPED:          return ServiceStatus::Stopped;
        case SERVICE_START_PENDING:    return ServiceStatus::StartPending;
        case SERVICE_STOP_PENDING:     return ServiceStatus::StopPending;
        case SERVICE_PAUSED:           return ServiceStatus::Paused;
        case SERVICE_PAUSE_PENDING:    return ServiceStatus::PausePending;
        case SERVICE_CONTINUE_PENDING: return ServiceStatus::ContinuePending;
        default:                       return ServiceStatus::Stopped;
    }
#else
    return simulatedStatus_;
#endif
}

void HostServiceManager::setTrayState(TrayIconState state) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    trayState_ = state;
}

TrayIconState HostServiceManager::getTrayState() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return trayState_;
}

uint32_t HostServiceManager::getActiveGuestCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeGuests_;
}

void HostServiceManager::setActiveGuestCount(uint32_t count) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    activeGuests_ = count;
    trayState_ = (count > 0) ? TrayIconState::Streaming : TrayIconState::Ready;
}

void HostServiceManager::postNotification(TrayNotification notification) {
    std::lock_guard<std::mutex> lock(mutex_);
    MIST_LOG_INFO(common::SubsystemId::Platform,
                  "Tray Notification: [" + notification.title + "] " + notification.message);
    notifications_.push_back(std::move(notification));
}

std::vector<TrayNotification> HostServiceManager::getNotificationHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return notifications_;
}

} // namespace mist::platform::windows
