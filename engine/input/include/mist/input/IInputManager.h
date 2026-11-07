#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <variant>

namespace mist::input {

enum class KeyAction : uint8_t {
    Press = 0,
    Release
};

struct KeyEvent {
    uint32_t virtualKey{0};
    uint32_t scanCode{0};
    KeyAction action{KeyAction::Press};
    bool isExtended{false};
};

enum class MouseButton : uint8_t {
    None = 0,
    Left,
    Right,
    Middle,
    X1,
    X2
};

struct MouseMoveEvent {
    int32_t deltaX{0};
    int32_t deltaY{0};
    bool isAbsolute{false};
    int32_t absoluteX{0};
    int32_t absoluteY{0};
};

struct MouseButtonEvent {
    MouseButton button{MouseButton::None};
    bool isDown{false};
};

struct MouseWheelEvent {
    int32_t wheelDelta{0};
};

struct GamepadState {
    uint32_t index{0};
    uint16_t buttons{0};
    uint8_t leftTrigger{0};
    uint8_t rightTrigger{0};
    int16_t thumbLX{0};
    int16_t thumbLY{0};
    int16_t thumbRX{0};
    int16_t thumbRY{0};
};

using InputEvent = std::variant<KeyEvent, MouseMoveEvent, MouseButtonEvent, MouseWheelEvent, GamepadState>;

class IInputManager {
public:
    virtual ~IInputManager() = default;

    [[nodiscard]] virtual common::Result<void> initialize() = 0;
    [[nodiscard]] virtual common::Result<void> injectEvent(const InputEvent& event) = 0;
    [[nodiscard]] virtual common::Result<void> reset() = 0;
    [[nodiscard]] virtual common::Result<void> shutdown() = 0;
};

} // namespace mist::input
