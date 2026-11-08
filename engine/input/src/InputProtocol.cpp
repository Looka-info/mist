#include "mist/input/InputProtocol.h"
#include <cstring>
#include <cmath>

namespace mist::input {

namespace {

#pragma pack(push, 1)

struct KeyPayload {
    uint32_t virtualKey{0};
    uint32_t scanCode{0};
    uint8_t action{0};
    uint8_t isExtended{0};
    uint16_t reserved{0};
};

struct MouseMovePayload {
    int32_t deltaX{0};
    int32_t deltaY{0};
    uint8_t isAbsolute{0};
    uint8_t reserved[3]{0, 0, 0};
    float normalizedX{0.0f};
    float normalizedY{0.0f};
};

struct MouseButtonPayload {
    uint8_t button{0};
    uint8_t isDown{0};
    uint16_t reserved{0};
};

struct MouseWheelPayload {
    int32_t wheelDeltaX{0};
    int32_t wheelDeltaY{0};
};

struct GamepadPayload {
    uint32_t index{0};
    uint16_t buttons{0};
    uint8_t leftTrigger{0};
    uint8_t rightTrigger{0};
    int16_t thumbLX{0};
    int16_t thumbLY{0};
    int16_t thumbRX{0};
    int16_t thumbRY{0};
    uint8_t isConnected{0};
    uint8_t reserved[3]{0, 0, 0};
};

struct PermissionPayload {
    uint8_t action{0};
    uint8_t requestedFeatures{0};
    uint16_t reserved{0};
};

#pragma pack(pop)

template <typename PayloadT>
std::vector<uint8_t> packMessage(InputPacketHeader header, InputType type, const PayloadT& payload) {
    header.inputType = static_cast<uint8_t>(type);
    std::vector<uint8_t> buffer(sizeof(InputPacketHeader) + sizeof(PayloadT));
    std::memcpy(buffer.data(), &header, sizeof(InputPacketHeader));
    std::memcpy(buffer.data() + sizeof(InputPacketHeader), &payload, sizeof(PayloadT));
    return buffer;
}

} // namespace

std::vector<uint8_t> InputProtocol::serializeKeyEvent(const InputPacketHeader& header, const KeyEvent& event) {
    KeyPayload payload{};
    payload.virtualKey = event.virtualKey;
    payload.scanCode = event.scanCode;
    payload.action = static_cast<uint8_t>(event.action);
    payload.isExtended = event.isExtended ? 1 : 0;
    return packMessage(header, InputType::KeyEvent, payload);
}

std::vector<uint8_t> InputProtocol::serializeMouseMoveEvent(const InputPacketHeader& header, const MouseMoveEvent& event) {
    MouseMovePayload payload{};
    payload.deltaX = event.deltaX;
    payload.deltaY = event.deltaY;
    payload.isAbsolute = event.isAbsolute ? 1 : 0;
    payload.normalizedX = event.normalizedX;
    payload.normalizedY = event.normalizedY;
    return packMessage(header, InputType::MouseMoveEvent, payload);
}

std::vector<uint8_t> InputProtocol::serializeMouseButtonEvent(const InputPacketHeader& header, const MouseButtonEvent& event) {
    MouseButtonPayload payload{};
    payload.button = static_cast<uint8_t>(event.button);
    payload.isDown = event.isDown ? 1 : 0;
    return packMessage(header, InputType::MouseButtonEvent, payload);
}

std::vector<uint8_t> InputProtocol::serializeMouseWheelEvent(const InputPacketHeader& header, const MouseWheelEvent& event) {
    MouseWheelPayload payload{};
    payload.wheelDeltaX = event.wheelDeltaX;
    payload.wheelDeltaY = event.wheelDeltaY;
    return packMessage(header, InputType::MouseWheelEvent, payload);
}

std::vector<uint8_t> InputProtocol::serializeGamepadState(const InputPacketHeader& header, const GamepadState& state) {
    GamepadPayload payload{};
    payload.index = state.index;
    payload.buttons = state.buttons;
    payload.leftTrigger = state.leftTrigger;
    payload.rightTrigger = state.rightTrigger;
    payload.thumbLX = state.thumbLX;
    payload.thumbLY = state.thumbLY;
    payload.thumbRX = state.thumbRX;
    payload.thumbRY = state.thumbRY;
    payload.isConnected = state.isConnected ? 1 : 0;
    return packMessage(header, InputType::GamepadState, payload);
}

std::vector<uint8_t> InputProtocol::serializePermissionMessage(const InputPacketHeader& header, const PermissionMessage& message) {
    PermissionPayload payload{};
    payload.action = static_cast<uint8_t>(message.action);
    payload.requestedFeatures = message.requestedFeatures;
    InputType type = InputType::PermissionRequest;
    if (message.action == PermissionAction::Granted) {
        type = InputType::PermissionGrant;
    } else if (message.action == PermissionAction::Revoked || message.action == PermissionAction::Denied) {
        type = InputType::PermissionRevoke;
    }
    return packMessage(header, type, payload);
}

std::vector<uint8_t> InputProtocol::serializeResetState(const InputPacketHeader& header) {
    InputPacketHeader hdr = header;
    hdr.inputType = static_cast<uint8_t>(InputType::ResetState);
    std::vector<uint8_t> buffer(sizeof(InputPacketHeader));
    std::memcpy(buffer.data(), &hdr, sizeof(InputPacketHeader));
    return buffer;
}

common::Result<std::pair<InputPacketHeader, InputEvent>> InputProtocol::deserialize(std::span<const uint8_t> bytes) {
    if (bytes.size() < sizeof(InputPacketHeader)) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Input packet too small to contain header");
    }
    if (bytes.size() > MAX_INPUT_PACKET_SIZE) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Input packet exceeds maximum permitted size");
    }

    InputPacketHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(InputPacketHeader));

    if (header.version != INPUT_PROTOCOL_VERSION) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Unsupported input protocol version");
    }

    const uint8_t* payloadPtr = bytes.data() + sizeof(InputPacketHeader);
    const size_t payloadSize = bytes.size() - sizeof(InputPacketHeader);

    auto type = static_cast<InputType>(header.inputType);

    switch (type) {
        case InputType::KeyEvent: {
            if (payloadSize != sizeof(KeyPayload)) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Mismatched KeyEvent payload size");
            }
            KeyPayload payload{};
            std::memcpy(&payload, payloadPtr, sizeof(KeyPayload));

            KeyEvent ev{};
            ev.virtualKey = payload.virtualKey;
            ev.scanCode = payload.scanCode;
            ev.action = (payload.action == 0) ? KeyAction::Press : KeyAction::Release;
            ev.isExtended = (payload.isExtended != 0);
            return std::make_pair(header, InputEvent(ev));
        }

        case InputType::MouseMoveEvent: {
            if (payloadSize != sizeof(MouseMovePayload)) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Mismatched MouseMoveEvent payload size");
            }
            MouseMovePayload payload{};
            std::memcpy(&payload, payloadPtr, sizeof(MouseMovePayload));

            MouseMoveEvent ev{};
            ev.deltaX = payload.deltaX;
            ev.deltaY = payload.deltaY;
            ev.isAbsolute = (payload.isAbsolute != 0);
            ev.normalizedX = std::isnan(payload.normalizedX) ? 0.0f : payload.normalizedX;
            ev.normalizedY = std::isnan(payload.normalizedY) ? 0.0f : payload.normalizedY;
            return std::make_pair(header, InputEvent(ev));
        }

        case InputType::MouseButtonEvent: {
            if (payloadSize != sizeof(MouseButtonPayload)) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Mismatched MouseButtonEvent payload size");
            }
            MouseButtonPayload payload{};
            std::memcpy(&payload, payloadPtr, sizeof(MouseButtonPayload));

            if (payload.button > static_cast<uint8_t>(MouseButton::X2)) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Invalid MouseButton ID");
            }

            MouseButtonEvent ev{};
            ev.button = static_cast<MouseButton>(payload.button);
            ev.isDown = (payload.isDown != 0);
            return std::make_pair(header, InputEvent(ev));
        }

        case InputType::MouseWheelEvent: {
            if (payloadSize != sizeof(MouseWheelPayload)) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Mismatched MouseWheelEvent payload size");
            }
            MouseWheelPayload payload{};
            std::memcpy(&payload, payloadPtr, sizeof(MouseWheelPayload));

            MouseWheelEvent ev{};
            ev.wheelDeltaX = payload.wheelDeltaX;
            ev.wheelDeltaY = payload.wheelDeltaY;
            return std::make_pair(header, InputEvent(ev));
        }

        case InputType::GamepadState: {
            if (payloadSize != sizeof(GamepadPayload)) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Mismatched GamepadState payload size");
            }
            GamepadPayload payload{};
            std::memcpy(&payload, payloadPtr, sizeof(GamepadPayload));

            GamepadState st{};
            st.index = payload.index;
            st.buttons = payload.buttons;
            st.leftTrigger = payload.leftTrigger;
            st.rightTrigger = payload.rightTrigger;
            st.thumbLX = payload.thumbLX;
            st.thumbLY = payload.thumbLY;
            st.thumbRX = payload.thumbRX;
            st.thumbRY = payload.thumbRY;
            st.isConnected = (payload.isConnected != 0);
            return std::make_pair(header, InputEvent(st));
        }

        case InputType::PermissionRequest:
        case InputType::PermissionGrant:
        case InputType::PermissionRevoke: {
            if (payloadSize != sizeof(PermissionPayload)) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Mismatched PermissionMessage payload size");
            }
            PermissionPayload payload{};
            std::memcpy(&payload, payloadPtr, sizeof(PermissionPayload));

            PermissionMessage msg{};
            msg.action = static_cast<PermissionAction>(payload.action);
            msg.requestedFeatures = payload.requestedFeatures;
            return std::make_pair(header, InputEvent(msg));
        }

        case InputType::ResetState: {
            if (payloadSize != 0) {
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "ResetState must have empty payload");
            }
            PermissionMessage msg{};
            msg.action = PermissionAction::Revoked;
            return std::make_pair(header, InputEvent(msg));
        }

        default:
            return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument, "Unknown input packet type");
    }
}

} // namespace mist::input