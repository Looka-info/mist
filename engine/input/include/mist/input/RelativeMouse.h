#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include <cstdint>
#include <mutex>
#include <functional>

namespace mist::input {

enum class MouseCaptureMode : uint8_t {
    Disabled = 0,
    Confined = 1,
    LockedRelative = 2
};

struct RelativeMouseConfig {
    float sensitivity{1.0f};           // User sensitivity multiplier (0.1 to 10.0)
    float clientDpiScale{1.0f};        // Client DPI scale (e.g. 1.25, 1.5, 2.0)
    float hostDpiScale{1.0f};          // Host DPI scale
    bool ungrabOnEscape{true};         // Release capture on Escape key
    bool ungrabOnCtrlAlt{true};        // Release capture on Ctrl+Alt key combination
    bool simulationOnly{false};        // When true, avoids actual Win32 ClipCursor / ShowCursor
};

using MouseModeChangedCallback = std::function<void(MouseCaptureMode newMode)>;

class RelativeMouseController {
public:
    explicit RelativeMouseController(RelativeMouseConfig config = {}) noexcept;
    ~RelativeMouseController();

    // Mode transitions
    common::Result<void> setCaptureMode(MouseCaptureMode mode, int viewportWidth = 1920, int viewportHeight = 1080);
    common::Result<void> lockCursor(int viewportWidth = 1920, int viewportHeight = 1080);
    common::Result<void> unlockCursor();
    common::Result<void> toggleLock(int viewportWidth = 1920, int viewportHeight = 1080);

    [[nodiscard]] MouseCaptureMode getCaptureMode() const noexcept;
    [[nodiscard]] bool isLocked() const noexcept;

    // Viewport geometry update
    void updateViewport(int x, int y, int width, int height);

    // Delta processing & DPI normalization
    // Converts raw hardware mouse counts (from Win32 RAWINPUT / WM_INPUT) into normalized relative MouseMoveEvent
    [[nodiscard]] MouseMoveEvent processRawDelta(int32_t rawDeltaX, int32_t rawDeltaY) const;

    // Hotkey / ungrab checks
    // Returns true if ungrab condition is met and unlocks cursor
    bool checkUngrabHotkey(uint32_t virtualKey, bool ctrlDown, bool altDown);

    // Configuration
    void setSensitivity(float s) noexcept;
    [[nodiscard]] float getSensitivity() const noexcept;

    void setDpiScales(float clientDpi, float hostDpi) noexcept;
    [[nodiscard]] float getClientDpiScale() const noexcept;
    [[nodiscard]] float getHostDpiScale() const noexcept;

    void setModeChangedCallback(MouseModeChangedCallback cb);

    // Win32 HWND binding (optional, used for real window confinement)
    void attachWindow(void* hwnd);

private:
    void applyWin32Capture(MouseCaptureMode mode);
    void releaseWin32Capture();

    mutable std::mutex mutex_;
    RelativeMouseConfig config_;
    MouseCaptureMode mode_{MouseCaptureMode::Disabled};
    int vpX_{0};
    int vpY_{0};
    int vpWidth_{1920};
    int vpHeight_{1080};
    void* attachedHwnd_{nullptr};
    bool cursorHidden_{false};
    MouseModeChangedCallback modeCallback_;
};

} // namespace mist::input
