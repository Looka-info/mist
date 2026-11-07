#include "mist/platform/windows/PlatformWindows.h"

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <sstream>

namespace mist::platform::windows {

common::Error win32ErrorToMistError(uint32_t win32ErrorCode, common::SubsystemId subsystem, std::string_view context) {
    if (win32ErrorCode == 0) {
        return common::Error(subsystem, common::ErrorCode::Success, "");
    }

    common::ErrorCode mappedCode = common::ErrorCode::InternalError;
    switch (win32ErrorCode) {
        case 2:  // ERROR_FILE_NOT_FOUND
        case 3:  // ERROR_PATH_NOT_FOUND
            mappedCode = common::ErrorCode::NotFound;
            break;
        case 5:  // ERROR_ACCESS_DENIED
            mappedCode = common::ErrorCode::PermissionDenied;
            break;
        case 8:  // ERROR_NOT_ENOUGH_MEMORY
        case 14: // ERROR_OUTOFMEMORY
            mappedCode = common::ErrorCode::ResourceExhausted;
            break;
        case 87: // ERROR_INVALID_PARAMETER
            mappedCode = common::ErrorCode::InvalidArgument;
            break;
        case 121: // ERROR_SEM_TIMEOUT
        case 1460: // ERROR_TIMEOUT
            mappedCode = common::ErrorCode::Timeout;
            break;
        default:
            mappedCode = common::ErrorCode::InternalError;
            break;
    }

    std::string msg;
#if defined(_WIN32)
    char* buffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        win32ErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr
    );
    if (size > 0 && buffer != nullptr) {
        // Strip trailing newlines
        while (size > 0 && (buffer[size - 1] == '\r' || buffer[size - 1] == '\n')) {
            buffer[--size] = '\0';
        }
        msg = buffer;
        LocalFree(buffer);
    } else {
        msg = "Unknown Win32 Error";
    }
#else
    msg = "Win32 error simulation";
#endif

    std::ostringstream oss;
    if (!context.empty()) {
        oss << context << ": ";
    }
    oss << msg;

    return common::Error(subsystem, mappedCode, oss.str(), static_cast<int32_t>(win32ErrorCode));
}

common::Error hresultToMistError(long hr, common::SubsystemId subsystem, std::string_view context) {
    if (hr >= 0) {
        return common::Error(subsystem, common::ErrorCode::Success, "");
    }

    common::ErrorCode code = common::ErrorCode::InternalError;
    if (hr == static_cast<long>(0x80070057)) { // E_INVALIDARG
        code = common::ErrorCode::InvalidArgument;
    } else if (hr == static_cast<long>(0x8007000E)) { // E_OUTOFMEMORY
        code = common::ErrorCode::ResourceExhausted;
    } else if (hr == static_cast<long>(0x80004001)) { // E_NOTIMPL
        code = common::ErrorCode::NotImplemented;
    } else if (hr == static_cast<long>(0x80004005)) { // E_FAIL
        code = common::ErrorCode::InternalError;
    }

    std::ostringstream oss;
    if (!context.empty()) {
        oss << context << " ";
    }
    oss << "(HRESULT: 0x" << std::hex << static_cast<uint32_t>(hr) << std::dec << ")";

    return common::Error(subsystem, code, oss.str(), static_cast<int32_t>(hr));
}

common::Result<void> initializeProcessDpiAwareness() noexcept {
#if defined(_WIN32)
    // Dynamically query SetProcessDpiAwarenessContext to support all Windows SDK headers
    using PFN_SetProcessDpiAwarenessContext = BOOL(WINAPI*)(HANDLE);
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        auto pfn = reinterpret_cast<PFN_SetProcessDpiAwarenessContext>(
            reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
        );
        if (pfn) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is ((HANDLE)-4)
            HANDLE perMonitorV2 = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4));
            if (pfn(perMonitorV2)) {
                return common::Result<void>::success();
            }
        }
    }
    // Fallback to legacy DPI awareness
    SetProcessDPIAware();
#endif
    return common::Result<void>::success();
}

HighResolutionTimer::HighResolutionTimer() noexcept {
#if defined(_WIN32)
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    frequency_ = freq.QuadPart;
#else
    frequency_ = 1000000000LL;
#endif
    reset();
}

void HighResolutionTimer::reset() noexcept {
#if defined(_WIN32)
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    startTime_ = now.QuadPart;
#else
    startTime_ = 0;
#endif
}

uint64_t HighResolutionTimer::elapsedNanoseconds() const noexcept {
#if defined(_WIN32)
    if (frequency_ <= 0) return 0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    int64_t elapsed = now.QuadPart - startTime_;
    return static_cast<uint64_t>((elapsed * 1000000000LL) / frequency_);
#else
    return 0;
#endif
}

double HighResolutionTimer::elapsedMicroseconds() const noexcept {
    return static_cast<double>(elapsedNanoseconds()) / 1000.0;
}

double HighResolutionTimer::elapsedMilliseconds() const noexcept {
    return static_cast<double>(elapsedNanoseconds()) / 1000000.0;
}

} // namespace mist::platform::windows
