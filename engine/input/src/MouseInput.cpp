#include "mist/input/MouseInput.h"
#include <algorithm>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mist::input {

WindowsMouseInjector::WindowsMouseInjector(bool simulationOnly) noexcept
    : simulationOnly_(simulationOnly) {}

WindowsMouseInjector::~WindowsMouseInjector() {
    releaseAllButtons();
}

common::Result<void> WindowsMouseInjector::injectMove(const MouseMoveEvent& event) {
    if (simulationOnly_) {
        return common::Result<void>::success();
    }

    INPUT input{};
    input.type = INPUT_MOUSE;

    if (event.isAbsolute) {
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        float normX = std::clamp(event.normalizedX, 0.0f, 1.0f);
        float normY = std::clamp(event.normalizedY, 0.0f, 1.0f);
        input.mi.dx = static_cast<LONG>(normX * 65535.0f);
        input.mi.dy = static_cast<LONG>(normY * 65535.0f);
    } else {
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = event.deltaX;
        input.mi.dy = event.deltaY;
    }

    UINT sent = SendInput(1, &input, sizeof(INPUT));
    if (sent != 1) {
        DWORD err = GetLastError();
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InternalError,
                             "SendInput mouse move failed: " + std::to_string(err));
    }

    return common::Result<void>::success();
}

common::Result<void> WindowsMouseInjector::injectButton(const MouseButtonEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(event.button));
    if (event.isDown) {
        heldButtonsMask_ |= bit;
    } else {
        heldButtonsMask_ &= static_cast<uint8_t>(~bit);
    }

    if (simulationOnly_) {
        return common::Result<void>::success();
    }

    INPUT input{};
    input.type = INPUT_MOUSE;

    switch (event.button) {
        case MouseButton::Left:
            input.mi.dwFlags = event.isDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case MouseButton::Right:
            input.mi.dwFlags = event.isDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case MouseButton::Middle:
            input.mi.dwFlags = event.isDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
        case MouseButton::X1:
            input.mi.dwFlags = event.isDown ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            input.mi.mouseData = XBUTTON1;
            break;
        case MouseButton::X2:
            input.mi.dwFlags = event.isDown ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            input.mi.mouseData = XBUTTON2;
            break;
        default:
            return common::Result<void>::success();
    }

    UINT sent = SendInput(1, &input, sizeof(INPUT));
    if (sent != 1) {
        DWORD err = GetLastError();
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InternalError,
                             "SendInput mouse button failed: " + std::to_string(err));
    }

    return common::Result<void>::success();
}

common::Result<void> WindowsMouseInjector::injectWheel(const MouseWheelEvent& event) {
    if (simulationOnly_) {
        return common::Result<void>::success();
    }

    std::vector<INPUT> inputs;

    if (event.wheelDeltaY != 0) {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(event.wheelDeltaY);
        inputs.push_back(input);
    }

    if (event.wheelDeltaX != 0) {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        input.mi.mouseData = static_cast<DWORD>(event.wheelDeltaX);
        inputs.push_back(input);
    }

    if (!inputs.empty()) {
        UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        if (sent != inputs.size()) {
            DWORD err = GetLastError();
            return common::Error(common::SubsystemId::Input, common::ErrorCode::InternalError,
                                 "SendInput mouse wheel failed: " + std::to_string(err));
        }
    }

    return common::Result<void>::success();
}

void WindowsMouseInjector::releaseAllButtons() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (heldButtonsMask_ == 0) {
        return;
    }

    if (!simulationOnly_) {
        std::vector<INPUT> inputs;

        auto checkAndRelease = [&](MouseButton btn, DWORD flag, DWORD mouseData = 0) {
            auto bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(btn));
            if (heldButtonsMask_ & bit) {
                INPUT input{};
                input.type = INPUT_MOUSE;
                input.mi.dwFlags = flag;
                input.mi.mouseData = mouseData;
                inputs.push_back(input);
            }
        };

        checkAndRelease(MouseButton::Left, MOUSEEVENTF_LEFTUP);
        checkAndRelease(MouseButton::Right, MOUSEEVENTF_RIGHTUP);
        checkAndRelease(MouseButton::Middle, MOUSEEVENTF_MIDDLEUP);
        checkAndRelease(MouseButton::X1, MOUSEEVENTF_XUP, XBUTTON1);
        checkAndRelease(MouseButton::X2, MOUSEEVENTF_XUP, XBUTTON2);

        if (!inputs.empty()) {
            SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        }
    }

    heldButtonsMask_ = 0;
}

uint8_t WindowsMouseInjector::getHeldButtonMask() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return heldButtonsMask_;
}

} // namespace mist::input