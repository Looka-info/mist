#include "mist/input/KeyboardInput.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mist::input {

WindowsKeyboardInjector::WindowsKeyboardInjector(bool simulationOnly) noexcept
    : simulationOnly_(simulationOnly) {}

WindowsKeyboardInjector::~WindowsKeyboardInjector() {
    releaseAllKeys();
}

common::Result<void> WindowsKeyboardInjector::injectKey(const KeyEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t scan = event.scanCode;
    if (scan == 0 && event.virtualKey != 0) {
        scan = MapVirtualKeyW(event.virtualKey, MAPVK_VK_TO_VSC);
    }

    if (event.action == KeyAction::Press) {
        heldScanCodes_.insert(scan);
    } else {
        heldScanCodes_.erase(scan);
    }

    if (simulationOnly_) {
        return common::Result<void>::success();
    }

    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = static_cast<WORD>(scan);
    input.ki.wVk = 0; // Prefer scancodes
    input.ki.dwFlags = KEYEVENTF_SCANCODE;

    if (event.isExtended) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    if (event.action == KeyAction::Release) {
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
    }

    UINT sent = SendInput(1, &input, sizeof(INPUT));
    if (sent != 1) {
        DWORD err = GetLastError();
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InternalError,
                             "SendInput failed with Win32 error code: " + std::to_string(err));
    }

    return common::Result<void>::success();
}

void WindowsKeyboardInjector::releaseAllKeys() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (heldScanCodes_.empty()) {
        return;
    }

    if (!simulationOnly_) {
        std::vector<INPUT> inputs;
        inputs.reserve(heldScanCodes_.size());

        for (uint32_t scan : heldScanCodes_) {
            INPUT input{};
            input.type = INPUT_KEYBOARD;
            input.ki.wScan = static_cast<WORD>(scan);
            input.ki.wVk = 0;
            input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
            inputs.push_back(input);
        }

        if (!inputs.empty()) {
            SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        }
    }

    heldScanCodes_.clear();
}

std::vector<uint32_t> WindowsKeyboardInjector::getHeldScanCodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<uint32_t>(heldScanCodes_.begin(), heldScanCodes_.end());
}

} // namespace mist::input