#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <variant>

namespace mist::input {

constexpr uint16_t INPUT_PROTOCOL_VERSION = 1;
constexpr size_t MAX_SESSION_ID_LENGTH = 32;
constexpr size_t MAX_INPUT_PACKET_SIZE = 256;
constexpr size_t MAX_CLIPBOARD_TEXT_BYTES = 1024 * 1024; // 1 MB limit

enum class InputType : uint8_t {
    Unknown = 0,
    KeyEvent = 1,
    MouseMoveEvent = 2,
    MouseButtonEvent = 3,
    MouseWheelEvent = 4,
    GamepadState = 5,
    InputStateSnapshot = 6,
    PermissionRequest = 7,
    PermissionGrant = 8,
    PermissionRevoke = 9,
    ResetState = 10,
    ClipboardEvent = 11
};

[[nodiscard]] constexpr std::string_view inputTypeToString(InputType type) noexcept {
    switch (type) {
        case InputType::KeyEvent:           return "KeyEvent";
        case InputType::MouseMoveEvent:     return "MouseMoveEvent";
        case InputType::MouseButtonEvent:   return "MouseButtonEvent";
        case InputType::MouseWheelEvent:    return "MouseWheelEvent";
        case InputType::GamepadState:       return "GamepadState";
        case InputType::InputStateSnapshot: return "InputStateSnapshot";
        case InputType::PermissionRequest:  return "PermissionRequest";
        case InputType::PermissionGrant:    return "PermissionGrant";
        case InputType::PermissionRevoke:   return "PermissionRevoke";
        case InputType::ResetState:         return "ResetState";
        case InputType::ClipboardEvent:     return "ClipboardEvent";
        default:                            return "Unknown";
    }
}

enum class KeyAction : uint8_t {
    Press = 0,
    Release = 1
};

struct KeyEvent {
    uint32_t virtualKey{0};
    uint32_t scanCode{0};
    KeyAction action{KeyAction::Press};
    bool isExtended{false};
};

enum class MouseButton : uint8_t {
    None = 0,
    Left = 1,
    Right = 2,
    Middle = 3,
    X1 = 4,
    X2 = 5
};

struct MouseMoveEvent {
    int32_t deltaX{0};
    int32_t deltaY{0};
    bool isAbsolute{false};
    float normalizedX{0.0f}; // [0.0, 1.0] across host virtual desktop
    float normalizedY{0.0f}; // [0.0, 1.0] across host virtual desktop
};

struct MouseButtonEvent {
    MouseButton button{MouseButton::None};
    bool isDown{false};
};

struct MouseWheelEvent {
    int32_t wheelDeltaX{0}; // Horizontal scroll
    int32_t wheelDeltaY{0}; // Vertical scroll
};

struct GamepadState {
    uint32_t index{0};
    uint16_t buttons{0};
    uint8_t leftTrigger{0};   // 0 - 255
    uint8_t rightTrigger{0};  // 0 - 255
    int16_t thumbLX{0};       // -32768 to 32767
    int16_t thumbLY{0};       // -32768 to 32767
    int16_t thumbRX{0};       // -32768 to 32767
    int16_t thumbRY{0};       // -32768 to 32767
    bool isConnected{true};
};

struct GamepadVibration {
    uint32_t slotIndex{0};
    uint16_t leftMotorSpeed{0};   // 0 - 65535
    uint16_t rightMotorSpeed{0};  // 0 - 65535
};

enum class PermissionAction : uint8_t {
    Request = 0,
    Granted = 1,
    Denied = 2,
    Revoked = 3
};

struct PermissionMessage {
    PermissionAction action{PermissionAction::Request};
    uint8_t requestedFeatures{0x07}; // Bit 0: Keyboard, Bit 1: Mouse, Bit 2: Gamepad
};

struct ClipboardEvent {
    std::string text;
    uint64_t hash{0};
};

using InputEvent = std::variant<
    KeyEvent,
    MouseMoveEvent,
    MouseButtonEvent,
    MouseWheelEvent,
    GamepadState,
    PermissionMessage,
    ClipboardEvent
>;

#pragma pack(push, 1)
struct InputPacketHeader {
    uint16_t version{INPUT_PROTOCOL_VERSION};
    uint8_t inputType{static_cast<uint8_t>(InputType::Unknown)};
    uint8_t flags{0}; // Bit 0: Reliable/Transition, Bit 1: Snapshot, Bit 2: Emergency
    uint32_t sequenceNumber{0};
    uint64_t timestampNs{0};
    char sessionId[MAX_SESSION_ID_LENGTH]{0};
};
#pragma pack(pop)

static_assert(sizeof(InputPacketHeader) == 48, "InputPacketHeader must be exactly 48 bytes");

} // namespace mist::input