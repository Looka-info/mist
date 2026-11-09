#include "mist/session/PowerStateManager.h"
#include "mist/common/Logger.h"

namespace mist::session {

namespace {

constexpr uint32_t WIN32_PBT_APMSUSPEND        = 0x0004;
constexpr uint32_t WIN32_PBT_APMRESUMEAUTOMATIC = 0x0012;
constexpr uint32_t WIN32_PBT_APMRESUMESUSPEND   = 0x0007;

} // namespace

PowerStateManager::PowerStateManager() noexcept = default;

void PowerStateManager::registerSuspendHook(std::string name, PowerHook hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    suspendHooks_.push_back({std::move(name), std::move(hook)});
}

void PowerStateManager::registerResumeHook(std::string name, PowerHook hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    resumeHooks_.push_back({std::move(name), std::move(hook)});
}

void PowerStateManager::notifySuspend() {
    std::vector<RegisteredPowerHook> hooksToRun;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = PowerState::Suspending;
        suspendCount_++;
        hooksToRun = suspendHooks_;
    }

    MIST_LOG_INFO(common::SubsystemId::Session, "Host entering sleep/suspend. Executing suspend hooks...");

    for (const auto& hook : hooksToRun) {
        if (hook.callback) {
            try {
                hook.callback();
            } catch (...) {
                MIST_LOG_ERROR(common::SubsystemId::Session, "Exception in suspend hook: " + hook.name);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = PowerState::Suspended;
    }
}

void PowerStateManager::notifyResume() {
    std::vector<RegisteredPowerHook> hooksToRun;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = PowerState::Resuming;
        resumeCount_++;
        hooksToRun = resumeHooks_;
    }

    MIST_LOG_INFO(common::SubsystemId::Session, "Host resuming from sleep/suspend. Executing resume hooks...");

    for (const auto& hook : hooksToRun) {
        if (hook.callback) {
            try {
                hook.callback();
            } catch (...) {
                MIST_LOG_ERROR(common::SubsystemId::Session, "Exception in resume hook: " + hook.name);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = PowerState::Active;
    }
}

bool PowerStateManager::handlePowerBroadcast(uint32_t eventType) {
    switch (eventType) {
        case WIN32_PBT_APMSUSPEND:
            notifySuspend();
            return true;
        case WIN32_PBT_APMRESUMEAUTOMATIC:
        case WIN32_PBT_APMRESUMESUSPEND:
            notifyResume();
            return true;
        default:
            return false;
    }
}

PowerState PowerStateManager::getPowerState() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool PowerStateManager::isSuspended() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == PowerState::Suspended || state_ == PowerState::Suspending;
}

uint32_t PowerStateManager::getSuspendCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return suspendCount_;
}

uint32_t PowerStateManager::getResumeCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return resumeCount_;
}

void PowerStateManager::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = PowerState::Active;
    suspendCount_ = 0;
    resumeCount_ = 0;
}

} // namespace mist::session
