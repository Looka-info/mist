#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include <memory>
#include <string>

namespace mist::input {

// Dead zone thresholds per Microsoft XInput guidance
constexpr int16_t XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE = 7849;
constexpr int16_t XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE = 8689;
constexpr uint8_t XINPUT_GAMEPAD_TRIGGER_THRESHOLD = 30;

// Client-side physical controller capture
class ClientGamepadCapture {
public:
    ClientGamepadCapture() noexcept;
    ~ClientGamepadCapture();

    [[nodiscard]] common::Result<void> initialize();
    [[nodiscard]] common::Result<GamepadState> pollState(uint32_t userIndex = 0);
    [[nodiscard]] bool isAvailable() const noexcept { return isAvailable_; }

    // Dead zone filter utility
    [[nodiscard]] static GamepadState applyDeadZones(const GamepadState& raw);

private:
    void* xinputModule_{nullptr};
    using XInputGetStateFn = uint32_t(__stdcall*)(uint32_t, void*);
    XInputGetStateFn getStateFn_{nullptr};
    bool isAvailable_{false};
};

// Host-side virtual controller interface
class IVirtualGamepad {
public:
    virtual ~IVirtualGamepad() = default;
    [[nodiscard]] virtual common::Result<void> initialize() = 0;
    [[nodiscard]] virtual common::Result<void> updateState(const GamepadState& state) = 0;
    virtual void reset() = 0;
    [[nodiscard]] virtual bool isDriverAvailable() const noexcept = 0;
    [[nodiscard]] virtual std::string getDriverStatus() const = 0;
};

// Host-side virtual controller implementation (with ViGEm / VHF dynamic probe and safe fallback)
class HostVirtualGamepad : public IVirtualGamepad {
public:
    explicit HostVirtualGamepad(bool simulationOnly = false) noexcept;
    ~HostVirtualGamepad() override;

    [[nodiscard]] common::Result<void> initialize() override;
    [[nodiscard]] common::Result<void> updateState(const GamepadState& state) override;
    void reset() override;
    [[nodiscard]] bool isDriverAvailable() const noexcept override { return driverAvailable_; }
    [[nodiscard]] std::string getDriverStatus() const override;

    void setSimulationOnly(bool sim) noexcept { simulationOnly_ = sim; }

private:
    bool simulationOnly_{false};
    bool driverAvailable_{false};
    std::string driverStatus_{"Virtual Gamepad driver not detected (Optional ViGEmBus/VHF)"};
    GamepadState currentState_{};
};

} // namespace mist::input