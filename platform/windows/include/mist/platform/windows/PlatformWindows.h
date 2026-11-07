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

} // namespace mist::platform::windows
