#include "mist/input/ClientInputCapture.h"
#include <chrono>
#include <cstring>

namespace mist::input {

namespace {

uint64_t getNowNs() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace

ClientInputCapture::ClientInputCapture(std::string sessionId) noexcept
    : sessionId_(std::move(sessionId)) {}

void ClientInputCapture::onWindowFocusChanged(bool hasFocus) {
    bool prev = hasFocus_.exchange(hasFocus);
    if (prev && !hasFocus) {
        // Lost focus: send emergency release to ensure no keys or buttons get stuck on the host
        sendResetState();
    }
}

uint32_t ClientInputCapture::getNextSequenceNumber() noexcept {
    return sequenceCounter_.fetch_add(1, std::memory_order_relaxed) + 1;
}

void ClientInputCapture::sendPacket(InputType type, const std::vector<uint8_t>& packetBytes) {
    if (!sendCb_) {
        return;
    }

    // Continuous state (mouse move, gamepad state) goes to low-latency "input-state" channel
    // Discrete events (keys, clicks, permissions, reset) go to reliable "input-events" channel
    const std::string& channel = (type == InputType::MouseMoveEvent || type == InputType::GamepadState)
                                     ? "input-state"
                                     : "input-events";

    sendCb_(channel, packetBytes);
}

void ClientInputCapture::onKeyDown(uint32_t vk, uint32_t scanCode, bool isExtended) {
    if (!hasFocus_.load()) return;

    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    KeyEvent ev{vk, scanCode, KeyAction::Press, isExtended};
    auto bytes = InputProtocol::serializeKeyEvent(hdr, ev);
    sendPacket(InputType::KeyEvent, bytes);
}

void ClientInputCapture::onKeyUp(uint32_t vk, uint32_t scanCode, bool isExtended) {
    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    KeyEvent ev{vk, scanCode, KeyAction::Release, isExtended};
    auto bytes = InputProtocol::serializeKeyEvent(hdr, ev);
    sendPacket(InputType::KeyEvent, bytes);
}

void ClientInputCapture::onMouseMove(int32_t deltaX, int32_t deltaY, bool isAbsolute, float normX, float normY) {
    if (!hasFocus_.load()) return;

    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    MouseMoveEvent ev{deltaX, deltaY, isAbsolute, normX, normY};
    auto bytes = InputProtocol::serializeMouseMoveEvent(hdr, ev);
    sendPacket(InputType::MouseMoveEvent, bytes);
}

void ClientInputCapture::onMouseButton(MouseButton button, bool isDown) {
    if (!hasFocus_.load() && isDown) return;

    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    MouseButtonEvent ev{button, isDown};
    auto bytes = InputProtocol::serializeMouseButtonEvent(hdr, ev);
    sendPacket(InputType::MouseButtonEvent, bytes);
}

void ClientInputCapture::onMouseWheel(int32_t deltaX, int32_t deltaY) {
    if (!hasFocus_.load()) return;

    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    MouseWheelEvent ev{deltaX, deltaY};
    auto bytes = InputProtocol::serializeMouseWheelEvent(hdr, ev);
    sendPacket(InputType::MouseWheelEvent, bytes);
}

void ClientInputCapture::pollGamepad(ClientGamepadCapture& capture, uint32_t userIndex) {
    auto res = capture.pollState(userIndex);
    if (!res) return;

    const auto& rawState = res.value();
    if (!rawState.isConnected) return;

    auto filtered = ClientGamepadCapture::applyDeadZones(rawState);

    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    auto bytes = InputProtocol::serializeGamepadState(hdr, filtered);
    sendPacket(InputType::GamepadState, bytes);
}

void ClientInputCapture::sendPermissionRequest(uint8_t requestedFeatures) {
    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    PermissionMessage msg{PermissionAction::Request, requestedFeatures};
    auto bytes = InputProtocol::serializePermissionMessage(hdr, msg);
    sendPacket(InputType::PermissionRequest, bytes);
}

void ClientInputCapture::sendResetState() {
    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = getNextSequenceNumber();
    hdr.timestampNs = getNowNs();
    std::strncpy(hdr.sessionId, sessionId_.c_str(), sizeof(hdr.sessionId) - 1);

    auto bytes = InputProtocol::serializeResetState(hdr);
    sendPacket(InputType::ResetState, bytes);
}

} // namespace mist::input