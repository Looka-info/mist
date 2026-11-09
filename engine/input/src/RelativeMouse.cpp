#include "mist/input/RelativeMouse.h"
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mist::input {

RelativeMouseController::RelativeMouseController(RelativeMouseConfig config) noexcept
    : config_(config) {}

RelativeMouseController::~RelativeMouseController() {
    (void)unlockCursor();
}

common::Result<void> RelativeMouseController::setCaptureMode(MouseCaptureMode mode, int viewportWidth, int viewportHeight) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (mode_ == mode) {
        return common::Result<void>::success();
    }

    vpWidth_ = std::max(1, viewportWidth);
    vpHeight_ = std::max(1, viewportHeight);

    if (mode == MouseCaptureMode::Disabled) {
        releaseWin32Capture();
    } else {
        applyWin32Capture(mode);
    }

    mode_ = mode;

    if (modeCallback_) {
        modeCallback_(mode_);
    }

    return common::Result<void>::success();
}

common::Result<void> RelativeMouseController::lockCursor(int viewportWidth, int viewportHeight) {
    return setCaptureMode(MouseCaptureMode::LockedRelative, viewportWidth, viewportHeight);
}

common::Result<void> RelativeMouseController::unlockCursor() {
    return setCaptureMode(MouseCaptureMode::Disabled);
}

common::Result<void> RelativeMouseController::toggleLock(int viewportWidth, int viewportHeight) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto newMode = (mode_ == MouseCaptureMode::LockedRelative)
                       ? MouseCaptureMode::Disabled
                       : MouseCaptureMode::LockedRelative;

    vpWidth_ = std::max(1, viewportWidth);
    vpHeight_ = std::max(1, viewportHeight);

    if (newMode == MouseCaptureMode::Disabled) {
        releaseWin32Capture();
    } else {
        applyWin32Capture(newMode);
    }

    mode_ = newMode;

    if (modeCallback_) {
        modeCallback_(mode_);
    }

    return common::Result<void>::success();
}

MouseCaptureMode RelativeMouseController::getCaptureMode() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
}

bool RelativeMouseController::isLocked() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_ == MouseCaptureMode::LockedRelative;
}

void RelativeMouseController::updateViewport(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    vpX_ = x;
    vpY_ = y;
    vpWidth_ = std::max(1, width);
    vpHeight_ = std::max(1, height);

    if (mode_ != MouseCaptureMode::Disabled) {
        applyWin32Capture(mode_);
    }
}

MouseMoveEvent RelativeMouseController::processRawDelta(int32_t rawDeltaX, int32_t rawDeltaY) const {
    std::lock_guard<std::mutex> lock(mutex_);

    float dpiFactor = (config_.clientDpiScale > 0.0f)
                          ? (config_.hostDpiScale / config_.clientDpiScale)
                          : 1.0f;
    float effectiveMultiplier = config_.sensitivity * dpiFactor;

    int32_t normDx = static_cast<int32_t>(std::round(static_cast<float>(rawDeltaX) * effectiveMultiplier));
    int32_t normDy = static_cast<int32_t>(std::round(static_cast<float>(rawDeltaY) * effectiveMultiplier));

    MouseMoveEvent ev{};
    ev.isAbsolute = false;
    ev.deltaX = normDx;
    ev.deltaY = normDy;
    ev.normalizedX = 0.0f;
    ev.normalizedY = 0.0f;
    return ev;
}

bool RelativeMouseController::checkUngrabHotkey(uint32_t virtualKey, bool ctrlDown, bool altDown) {
    bool shouldUnlock = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (mode_ == MouseCaptureMode::Disabled) {
            return false;
        }

        if (config_.ungrabOnEscape && virtualKey == 0x1B /*VK_ESCAPE*/) {
            shouldUnlock = true;
        } else if (config_.ungrabOnCtrlAlt && ctrlDown && altDown) {
            shouldUnlock = true;
        }
    }

    if (shouldUnlock) {
        (void)unlockCursor();
        return true;
    }

    return false;
}

void RelativeMouseController::setSensitivity(float s) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.sensitivity = std::clamp(s, 0.05f, 20.0f);
}

float RelativeMouseController::getSensitivity() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.sensitivity;
}

void RelativeMouseController::setDpiScales(float clientDpi, float hostDpi) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.clientDpiScale = std::max(0.1f, clientDpi);
    config_.hostDpiScale = std::max(0.1f, hostDpi);
}

float RelativeMouseController::getClientDpiScale() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.clientDpiScale;
}

float RelativeMouseController::getHostDpiScale() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.hostDpiScale;
}

void RelativeMouseController::setModeChangedCallback(MouseModeChangedCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    modeCallback_ = std::move(cb);
}

void RelativeMouseController::attachWindow(void* hwnd) {
    std::lock_guard<std::mutex> lock(mutex_);
    attachedHwnd_ = hwnd;
}

void RelativeMouseController::applyWin32Capture(MouseCaptureMode mode) {
    if (config_.simulationOnly) {
        return;
    }

#ifdef _WIN32
    if (attachedHwnd_ != nullptr) {
        HWND hwnd = static_cast<HWND>(attachedHwnd_);
        RECT clientRect{};
        if (GetClientRect(hwnd, &clientRect)) {
            POINT topLeft{clientRect.left, clientRect.top};
            POINT bottomRight{clientRect.right, clientRect.bottom};
            ClientToScreen(hwnd, &topLeft);
            ClientToScreen(hwnd, &bottomRight);
            RECT clipRect{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
            ClipCursor(&clipRect);

            if (mode == MouseCaptureMode::LockedRelative) {
                int centerX = topLeft.x + (bottomRight.x - topLeft.x) / 2;
                int centerY = topLeft.y + (bottomRight.y - topLeft.y) / 2;
                SetCursorPos(centerX, centerY);

                if (!cursorHidden_) {
                    ShowCursor(FALSE);
                    cursorHidden_ = true;
                }
            }
        }
    }
#endif
}

void RelativeMouseController::releaseWin32Capture() {
    if (config_.simulationOnly) {
        return;
    }

#ifdef _WIN32
    ClipCursor(nullptr);
    if (cursorHidden_) {
        ShowCursor(TRUE);
        cursorHidden_ = false;
    }
#endif
}

} // namespace mist::input
