#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include "mist/input/InputProtocol.h"
#include "mist/input/GamepadInput.h"
#include <functional>
#include <string>
#include <atomic>
#include <mutex>
#include <span>

namespace mist::input {

using SendPacketCallback = std::function<void(const std::string& channelLabel, std::span<const uint8_t> data)>;

class ClientInputCapture {
public:
    explicit ClientInputCapture(std::string sessionId = "") noexcept;
    ~ClientInputCapture() = default;

    void setSessionId(std::string sessionId) { sessionId_ = std::move(sessionId); }
    void setSendCallback(SendPacketCallback cb) { sendCb_ = std::move(cb); }

    // Window focus tracking
    void onWindowFocusChanged(bool hasFocus);
    [[nodiscard]] bool hasFocus() const noexcept { return hasFocus_.load(); }

    // Win32 Window message translation
    void onKeyDown(uint32_t vk, uint32_t scanCode, bool isExtended);
    void onKeyUp(uint32_t vk, uint32_t scanCode, bool isExtended);

    void onMouseMove(int32_t deltaX, int32_t deltaY, bool isAbsolute = false, float normX = 0.0f, float normY = 0.0f);
    void onMouseButton(MouseButton button, bool isDown);
    void onMouseWheel(int32_t deltaX, int32_t deltaY);

    // Gamepad polling
    void pollGamepad(ClientGamepadCapture& capture, uint32_t userIndex = 0);

    // Permission negotiation
    void sendPermissionRequest(uint8_t requestedFeatures = 0x07);
    void sendResetState();

    [[nodiscard]] uint32_t getNextSequenceNumber() noexcept;

private:
    void sendPacket(InputType type, const std::vector<uint8_t>& packetBytes);

    std::string sessionId_;
    SendPacketCallback sendCb_;
    std::atomic<bool> hasFocus_{false};
    std::atomic<uint32_t> sequenceCounter_{0};
    mutable std::mutex sendMutex_;
};

} // namespace mist::input