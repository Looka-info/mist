#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include <span>
#include <vector>

namespace mist::input {

class InputProtocol {
public:
    // Serialization functions
    [[nodiscard]] static std::vector<uint8_t> serializeKeyEvent(
        const InputPacketHeader& header,
        const KeyEvent& event
    );

    [[nodiscard]] static std::vector<uint8_t> serializeMouseMoveEvent(
        const InputPacketHeader& header,
        const MouseMoveEvent& event
    );

    [[nodiscard]] static std::vector<uint8_t> serializeMouseButtonEvent(
        const InputPacketHeader& header,
        const MouseButtonEvent& event
    );

    [[nodiscard]] static std::vector<uint8_t> serializeMouseWheelEvent(
        const InputPacketHeader& header,
        const MouseWheelEvent& event
    );

    [[nodiscard]] static std::vector<uint8_t> serializeGamepadState(
        const InputPacketHeader& header,
        const GamepadState& state
    );

    [[nodiscard]] static std::vector<uint8_t> serializePermissionMessage(
        const InputPacketHeader& header,
        const PermissionMessage& message
    );

    [[nodiscard]] static std::vector<uint8_t> serializeResetState(
        const InputPacketHeader& header
    );

    [[nodiscard]] static std::vector<uint8_t> serializeClipboardEvent(
        const InputPacketHeader& header,
        const std::string& text
    );

    // Unified deserialization
    [[nodiscard]] static common::Result<std::pair<InputPacketHeader, InputEvent>> deserialize(
        std::span<const uint8_t> bytes
    );
};

} // namespace mist::input