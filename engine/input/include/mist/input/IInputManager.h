#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include <variant>

namespace mist::input {

class IInputManager {
public:
    virtual ~IInputManager() = default;

    [[nodiscard]] virtual common::Result<void> initialize() = 0;
    [[nodiscard]] virtual common::Result<void> injectEvent(const InputEvent& event) = 0;
    [[nodiscard]] virtual common::Result<void> reset() = 0;
    [[nodiscard]] virtual common::Result<void> shutdown() = 0;
};

} // namespace mist::input