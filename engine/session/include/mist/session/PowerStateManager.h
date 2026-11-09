#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include <mutex>

namespace mist::session {

enum class PowerState : uint8_t {
    Active = 0,
    Suspending,
    Suspended,
    Resuming
};

[[nodiscard]] constexpr std::string_view powerStateToString(PowerState s) noexcept {
    switch (s) {
        case PowerState::Active:     return "Active";
        case PowerState::Suspending: return "Suspending";
        case PowerState::Suspended:  return "Suspended";
        case PowerState::Resuming:   return "Resuming";
    }
    return "Unknown";
}

using PowerHook = std::function<void()>;

struct RegisteredPowerHook {
    std::string name;
    PowerHook callback;
};

class PowerStateManager {
public:
    PowerStateManager() noexcept;
    ~PowerStateManager() = default;

    // Register handlers called when host enters sleep/hibernation
    // (e.g. freeze encoder, release D3D11 swapchain, notify connected guests)
    void registerSuspendHook(std::string name, PowerHook hook);

    // Register handlers called when host wakes from sleep
    // (e.g. re-initialize D3D11 device, resume capture, trigger reconnection token invites)
    void registerResumeHook(std::string name, PowerHook hook);

    // Triggered when system enters suspend
    void notifySuspend();

    // Triggered when system wakes from suspend
    void notifyResume();

    // Win32 WM_POWERBROADCAST message handler
    // Returns true if event was handled
    bool handlePowerBroadcast(uint32_t eventType);

    // Status queries
    [[nodiscard]] PowerState getPowerState() const noexcept;
    [[nodiscard]] bool isSuspended() const noexcept;
    [[nodiscard]] uint32_t getSuspendCount() const noexcept;
    [[nodiscard]] uint32_t getResumeCount() const noexcept;

    // Reset power state tracking
    void reset() noexcept;

private:
    mutable std::mutex mutex_;
    PowerState state_{PowerState::Active};
    uint32_t suspendCount_{0};
    uint32_t resumeCount_{0};

    std::vector<RegisteredPowerHook> suspendHooks_;
    std::vector<RegisteredPowerHook> resumeHooks_;
};

} // namespace mist::session
