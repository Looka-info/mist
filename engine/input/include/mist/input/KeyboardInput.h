#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include <vector>
#include <unordered_set>
#include <mutex>
#include <memory>

namespace mist::input {

class IKeyboardInjector {
public:
    virtual ~IKeyboardInjector() = default;
    [[nodiscard]] virtual common::Result<void> injectKey(const KeyEvent& event) = 0;
    virtual void releaseAllKeys() = 0;
    [[nodiscard]] virtual std::vector<uint32_t> getHeldScanCodes() const = 0;
};

class WindowsKeyboardInjector : public IKeyboardInjector {
public:
    explicit WindowsKeyboardInjector(bool simulationOnly = false) noexcept;
    ~WindowsKeyboardInjector() override;

    [[nodiscard]] common::Result<void> injectKey(const KeyEvent& event) override;
    void releaseAllKeys() override;
    [[nodiscard]] std::vector<uint32_t> getHeldScanCodes() const override;

    [[nodiscard]] bool isSimulationOnly() const noexcept { return simulationOnly_; }
    void setSimulationOnly(bool sim) noexcept { simulationOnly_ = sim; }

private:
    bool simulationOnly_{false};
    mutable std::mutex mutex_;
    std::unordered_set<uint32_t> heldScanCodes_;
};

} // namespace mist::input