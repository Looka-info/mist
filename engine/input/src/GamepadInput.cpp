#include "mist/input/GamepadInput.h"
#include <cmath>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mist::input {

namespace {

#pragma pack(push, 1)
struct XInputGamepadStruct {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
};

struct XInputStateStruct {
    DWORD dwPacketNumber;
    XInputGamepadStruct Gamepad;
};
#pragma pack(pop)

} // namespace

ClientGamepadCapture::ClientGamepadCapture() noexcept = default;

ClientGamepadCapture::~ClientGamepadCapture() {
    if (xinputModule_) {
        FreeLibrary(static_cast<HMODULE>(xinputModule_));
        xinputModule_ = nullptr;
    }
}

common::Result<void> ClientGamepadCapture::initialize() {
    HMODULE mod = LoadLibraryW(L"xinput1_4.dll");
    if (!mod) {
        mod = LoadLibraryW(L"xinput9_1_0.dll");
    }

    if (!mod) {
        isAvailable_ = false;
        return common::Error(common::SubsystemId::Input, common::ErrorCode::HardwareUnavailable,
                             "XInput dynamic library could not be loaded");
    }

    xinputModule_ = mod;
    getStateFn_ = reinterpret_cast<XInputGetStateFn>(GetProcAddress(mod, "XInputGetState"));

    if (!getStateFn_) {
        FreeLibrary(mod);
        xinputModule_ = nullptr;
        isAvailable_ = false;
        return common::Error(common::SubsystemId::Input, common::ErrorCode::HardwareUnavailable,
                             "Failed to resolve XInputGetState symbol");
    }

    isAvailable_ = true;
    return common::Result<void>::success();
}

common::Result<GamepadState> ClientGamepadCapture::pollState(uint32_t userIndex) {
    if (!isAvailable_ || !getStateFn_) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::NotInitialized,
                             "ClientGamepadCapture not initialized");
    }

    XInputStateStruct state{};
    DWORD res = getStateFn_(userIndex, &state);

    GamepadState result{};
    result.index = userIndex;

    if (res == 0 /* ERROR_SUCCESS */) {
        result.isConnected = true;
        result.buttons = state.Gamepad.wButtons;
        result.leftTrigger = state.Gamepad.bLeftTrigger;
        result.rightTrigger = state.Gamepad.bRightTrigger;
        result.thumbLX = state.Gamepad.sThumbLX;
        result.thumbLY = state.Gamepad.sThumbLY;
        result.thumbRX = state.Gamepad.sThumbRX;
        result.thumbRY = state.Gamepad.sThumbRY;
        return result;
    }

    // Controller not connected or user index out of range
    result.isConnected = false;
    return result;
}

GamepadState ClientGamepadCapture::applyDeadZones(const GamepadState& raw) {
    GamepadState filtered = raw;

    // Left thumbstick deadzone
    if (std::abs(filtered.thumbLX) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        filtered.thumbLX = 0;
    }
    if (std::abs(filtered.thumbLY) < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        filtered.thumbLY = 0;
    }

    // Right thumbstick deadzone
    if (std::abs(filtered.thumbRX) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
        filtered.thumbRX = 0;
    }
    if (std::abs(filtered.thumbRY) < XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
        filtered.thumbRY = 0;
    }

    // Triggers threshold
    if (filtered.leftTrigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        filtered.leftTrigger = 0;
    }
    if (filtered.rightTrigger < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        filtered.rightTrigger = 0;
    }

    return filtered;
}

// Host Virtual Gamepad
HostVirtualGamepad::HostVirtualGamepad(bool simulationOnly) noexcept
    : simulationOnly_(simulationOnly) {}

HostVirtualGamepad::~HostVirtualGamepad() {
    reset();
}

common::Result<void> HostVirtualGamepad::initialize() {
    // Probe for ViGEmClient or VHF driver
    HMODULE vigemMod = LoadLibraryW(L"ViGEmClient.dll");
    if (vigemMod) {
        driverAvailable_ = true;
        driverStatus_ = "ViGEmBus client driver detected";
        FreeLibrary(vigemMod);
    } else {
        driverAvailable_ = false;
        driverStatus_ = "Virtual Gamepad driver not detected (Optional ViGEmBus/VHF)";
    }

    return common::Result<void>::success();
}

common::Result<void> HostVirtualGamepad::updateState(const GamepadState& state) {
    currentState_ = state;
    // In fallback mode or simulation, state is safely recorded without crashing
    return common::Result<void>::success();
}

void HostVirtualGamepad::reset() {
    currentState_ = GamepadState{};
    currentState_.isConnected = false;
}

std::string HostVirtualGamepad::getDriverStatus() const {
    return driverStatus_;
}

} // namespace mist::input