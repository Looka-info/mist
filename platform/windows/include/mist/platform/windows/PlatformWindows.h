#pragma once

#include "mist/common/Error.h"
#include "mist/common/Result.h"
#include <string>
#include <cstdint>

namespace mist::platform::windows {

// Translates a Win32 system error code (e.g. from GetLastError()) into a typed mist::common::Error
[[nodiscard]] common::Error win32ErrorToMistError(uint32_t win32ErrorCode, common::SubsystemId subsystem, std::string_view context);

// Translates an HRESULT into a typed mist::common::Error
[[nodiscard]] common::Error hresultToMistError(long hr, common::SubsystemId subsystem, std::string_view context);

// Sets process DPI awareness for crisp high-DPI rendering
common::Result<void> initializeProcessDpiAwareness() noexcept;

// High-resolution Windows platform timer (QueryPerformanceCounter wrapper)
class HighResolutionTimer {
public:
    HighResolutionTimer() noexcept;

    void reset() noexcept;
    [[nodiscard]] uint64_t elapsedNanoseconds() const noexcept;
    [[nodiscard]] double elapsedMicroseconds() const noexcept;
    [[nodiscard]] double elapsedMilliseconds() const noexcept;

private:
    int64_t frequency_{0};
    int64_t startTime_{0};
};

// RAII wrapper around timeBeginPeriod / timeEndPeriod for sub-millisecond scheduling precision
class ScopedTimerResolution {
public:
    explicit ScopedTimerResolution(uint32_t periodMs = 1) noexcept;
    ~ScopedTimerResolution() noexcept;
    ScopedTimerResolution(const ScopedTimerResolution&) = delete;
    ScopedTimerResolution& operator=(const ScopedTimerResolution&) = delete;
    ScopedTimerResolution(ScopedTimerResolution&& other) noexcept;
    ScopedTimerResolution& operator=(ScopedTimerResolution&& other) noexcept;

private:
    uint32_t periodMs_{0};
    bool active_{false};
};

// High-precision frame pacer combining coarse sleep with fine-grained QPC spin-wait
class FramePacer {
public:
    explicit FramePacer(uint32_t targetFps) noexcept;
    void reset() noexcept;
    void waitNextFrame() noexcept;

private:
    uint32_t targetFps_{60};
    int64_t intervalTicks_{0};
    int64_t nextFrameTicks_{0};
    int64_t frequency_{0};
    ScopedTimerResolution timerRes_{1};
};

} // namespace mist::platform::windows
