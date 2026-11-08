#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include <mutex>
#include <memory>

namespace mist::input {

class IMouseInjector {
public:
    virtual ~IMouseInjector() = default;
    [[nodiscard]] virtual common::Result<void> injectMove(const MouseMoveEvent& event) = 0;
    [[nodiscard]] virtual common::Result<void> injectButton(const MouseButtonEvent& event) = 0;
    [[nodiscard]] virtual common::Result<void> injectWheel(const MouseWheelEvent& event) = 0;
    virtual void releaseAllButtons() = 0;
    [[nodiscard]] virtual uint8_t getHeldButtonMask() const = 0;
};

class WindowsMouseInjector : public IMouseInjector {
public:
    explicit WindowsMouseInjector(bool simulationOnly = false) noexcept;
    ~WindowsMouseInjector() override;

    [[nodiscard]] common::Result<void> injectMove(const MouseMoveEvent& event) override;
    [[nodiscard]] common::Result<void> injectButton(const MouseButtonEvent& event) override;
    [[nodiscard]] common::Result<void> injectWheel(const MouseWheelEvent& event) override;
    void releaseAllButtons() override;
    [[nodiscard]] uint8_t getHeldButtonMask() const override;

    [[nodiscard]] bool isSimulationOnly() const noexcept { return simulationOnly_; }
    void setSimulationOnly(bool sim) noexcept { simulationOnly_ = sim; }

private:
    bool simulationOnly_{false};
    mutable std::mutex mutex_;
    uint8_t heldButtonsMask_{0};
};

} // namespace mist::input