#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include "mist/input/GamepadInput.h"
#include <string>
#include <array>
#include <optional>
#include <memory>
#include <mutex>
#include <functional>

namespace mist::input {

constexpr uint32_t MAX_VIRTUAL_GAMEPAD_SLOTS = 4;

struct VirtualGamepadSlot {
    uint32_t slotIndex{0};
    std::string boundGuestId;
    bool isOccupied{false};
    GamepadState lastState{};
    GamepadVibration currentVibration{};
    uint64_t statesReceived{0};
    uint64_t rumbleDispatched{0};
};

using RumbleNotificationCallback = std::function<void(const std::string& guestId, const GamepadVibration& vibration)>;

class MultiGuestGamepadManager {
public:
    MultiGuestGamepadManager() noexcept;
    ~MultiGuestGamepadManager();

    [[nodiscard]] common::Result<void> initialize();

    // Assigns a guest to the first available virtual gamepad slot (0..3)
    [[nodiscard]] common::Result<uint32_t> assignGuest(const std::string& guestId);

    // Releases a guest's slot and resets the virtual controller to neutral
    void releaseGuest(const std::string& guestId) noexcept;

    // Checks which slot is assigned to a guest
    [[nodiscard]] std::optional<uint32_t> getGuestSlot(const std::string& guestId) const noexcept;

    // Feeds incoming guest gamepad state into the virtual controller for that guest
    [[nodiscard]] common::Result<void> feedGuestGamepadState(
        const std::string& guestId,
        const GamepadState& state,
        bool hasGamepadPermission = true
    );

    // Sets vibration / rumble for a specific slot (e.g. from host game feedback)
    [[nodiscard]] common::Result<void> setSlotVibration(
        uint32_t slotIndex,
        uint16_t leftMotorSpeed,
        uint16_t rightMotorSpeed
    );

    // Sets callback for when host game sends rumble feedback back to guest
    void setRumbleCallback(RumbleNotificationCallback cb) { rumbleCb_ = std::move(cb); }

    [[nodiscard]] std::array<VirtualGamepadSlot, MAX_VIRTUAL_GAMEPAD_SLOTS> getSlotsSnapshot() const noexcept;
    [[nodiscard]] uint32_t getActiveSlotCount() const noexcept;
    [[nodiscard]] bool isDriverAvailable() const noexcept { return driverAvailable_; }

    void resetAll() noexcept;

private:
    mutable std::mutex mutex_;
    std::array<VirtualGamepadSlot, MAX_VIRTUAL_GAMEPAD_SLOTS> slots_{};
    std::array<std::unique_ptr<HostVirtualGamepad>, MAX_VIRTUAL_GAMEPAD_SLOTS> virtualGamepads_{};
    bool driverAvailable_{false};
    RumbleNotificationCallback rumbleCb_;
};

} // namespace mist::input
