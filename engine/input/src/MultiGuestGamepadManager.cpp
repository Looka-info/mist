#include "mist/input/MultiGuestGamepadManager.h"
#include <algorithm>

namespace mist::input {

MultiGuestGamepadManager::MultiGuestGamepadManager() noexcept {
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        slots_[i].slotIndex = i;
        slots_[i].isOccupied = false;
        virtualGamepads_[i] = std::make_unique<HostVirtualGamepad>();
    }
}

MultiGuestGamepadManager::~MultiGuestGamepadManager() {
    resetAll();
}

common::Result<void> MultiGuestGamepadManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    driverAvailable_ = true;
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        auto res = virtualGamepads_[i]->initialize();
        if (res && !virtualGamepads_[i]->isDriverAvailable()) {
            driverAvailable_ = false;
        }
    }
    return common::Result<void>::success();
}

common::Result<uint32_t> MultiGuestGamepadManager::assignGuest(const std::string& guestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (guestId.empty()) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Guest ID cannot be empty");
    }

    // Check if guest is already assigned to a slot
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        if (slots_[i].isOccupied && slots_[i].boundGuestId == guestId) {
            return i;
        }
    }

    // Find first available slot
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        if (!slots_[i].isOccupied) {
            slots_[i].isOccupied = true;
            slots_[i].boundGuestId = guestId;
            slots_[i].lastState = GamepadState{};
            slots_[i].lastState.index = i;
            slots_[i].lastState.isConnected = true;
            slots_[i].currentVibration = GamepadVibration{.slotIndex = i};
            slots_[i].statesReceived = 0;
            slots_[i].rumbleDispatched = 0;

            virtualGamepads_[i]->reset();
            return i;
        }
    }

    return common::Error(
        common::SubsystemId::Input,
        common::ErrorCode::ResourceExhausted,
        "All 4 virtual gamepad slots are currently occupied"
    );
}

void MultiGuestGamepadManager::releaseGuest(const std::string& guestId) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        if (slots_[i].isOccupied && slots_[i].boundGuestId == guestId) {
            slots_[i].isOccupied = false;
            slots_[i].boundGuestId.clear();
            slots_[i].lastState = GamepadState{};
            slots_[i].lastState.isConnected = false;
            slots_[i].currentVibration = GamepadVibration{.slotIndex = i};
            virtualGamepads_[i]->reset();
            break;
        }
    }
}

std::optional<uint32_t> MultiGuestGamepadManager::getGuestSlot(const std::string& guestId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        if (slots_[i].isOccupied && slots_[i].boundGuestId == guestId) {
            return i;
        }
    }
    return std::nullopt;
}

common::Result<void> MultiGuestGamepadManager::feedGuestGamepadState(
    const std::string& guestId,
    const GamepadState& state,
    bool hasGamepadPermission
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find slot bound to guest
    uint32_t slot = MAX_VIRTUAL_GAMEPAD_SLOTS;
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        if (slots_[i].isOccupied && slots_[i].boundGuestId == guestId) {
            slot = i;
            break;
        }
    }

    if (slot == MAX_VIRTUAL_GAMEPAD_SLOTS) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::NotFound, "Guest has no assigned gamepad slot");
    }

    if (!hasGamepadPermission) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::PermissionDenied, "Guest gamepad permission denied");
    }

    // Apply standard deadzone filtering
    GamepadState filtered = ClientGamepadCapture::applyDeadZones(state);
    filtered.index = slot;
    filtered.isConnected = true;

    slots_[slot].lastState = filtered;
    slots_[slot].statesReceived++;

    (void)virtualGamepads_[slot]->updateState(filtered);
    return common::Result<void>::success();
}

common::Result<void> MultiGuestGamepadManager::setSlotVibration(
    uint32_t slotIndex,
    uint16_t leftMotorSpeed,
    uint16_t rightMotorSpeed
) {
    std::string guestToNotify;
    GamepadVibration vib{};

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slotIndex >= MAX_VIRTUAL_GAMEPAD_SLOTS) {
            return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Slot index out of range (0..3)");
        }

        if (!slots_[slotIndex].isOccupied) {
            return common::Result<void>::success();
        }

        vib.slotIndex = slotIndex;
        vib.leftMotorSpeed = leftMotorSpeed;
        vib.rightMotorSpeed = rightMotorSpeed;

        slots_[slotIndex].currentVibration = vib;
        slots_[slotIndex].rumbleDispatched++;
        guestToNotify = slots_[slotIndex].boundGuestId;
    }

    // Dispatch rumble callback outside lock
    if (rumbleCb_ && !guestToNotify.empty()) {
        rumbleCb_(guestToNotify, vib);
    }

    return common::Result<void>::success();
}

std::array<VirtualGamepadSlot, MAX_VIRTUAL_GAMEPAD_SLOTS> MultiGuestGamepadManager::getSlotsSnapshot() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return slots_;
}

uint32_t MultiGuestGamepadManager::getActiveSlotCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        if (slots_[i].isOccupied) count++;
    }
    return count;
}

void MultiGuestGamepadManager::resetAll() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t i = 0; i < MAX_VIRTUAL_GAMEPAD_SLOTS; ++i) {
        slots_[i].isOccupied = false;
        slots_[i].boundGuestId.clear();
        slots_[i].lastState = GamepadState{};
        slots_[i].lastState.isConnected = false;
        slots_[i].currentVibration = GamepadVibration{.slotIndex = i};
        slots_[i].statesReceived = 0;
        slots_[i].rumbleDispatched = 0;
        virtualGamepads_[i]->reset();
    }
}

} // namespace mist::input
