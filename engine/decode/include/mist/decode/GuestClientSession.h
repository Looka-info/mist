#pragma once

#include "mist/decode/CrossPlatformDecoders.h"
#include "mist/decode/IVideoDecoder.h"
#include "mist/encode/IVideoEncoder.h"
#include "mist/input/InputTypes.h"
#include "mist/input/InputProtocol.h"
#include "mist/common/Result.h"
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>

namespace mist::decode {

enum class SessionLifecycleState : uint8_t {
    Disconnected = 0,
    Pairing,
    Authenticating,
    Signaling,
    ConnectingIce,
    Connected,
    Streaming,
    Reconnecting,
    Failed
};

[[nodiscard]] constexpr std::string_view sessionLifecycleStateToString(SessionLifecycleState s) noexcept {
    switch (s) {
        case SessionLifecycleState::Disconnected: return "Disconnected";
        case SessionLifecycleState::Pairing: return "Pairing";
        case SessionLifecycleState::Authenticating: return "Authenticating";
        case SessionLifecycleState::Signaling: return "Signaling";
        case SessionLifecycleState::ConnectingIce: return "ConnectingIce";
        case SessionLifecycleState::Connected: return "Connected";
        case SessionLifecycleState::Streaming: return "Streaming";
        case SessionLifecycleState::Reconnecting: return "Reconnecting";
        case SessionLifecycleState::Failed: return "Failed";
    }
    return "Unknown";
}

struct GuestClientConfig {
    ClientPlatform platform{ClientPlatform::Windows_D3D11VA};
    std::string signalingUrl{"ws://127.0.0.1:8080"};
    std::string sessionId{"mist-session-1"};
    std::string authToken{"mist-dev-token"};
    std::string guestId{"guest-default"};
    uint32_t targetWidth{1920};
    uint32_t targetHeight{1080};
    uint32_t targetFps{60};
    encode::VideoCodec preferredCodec{encode::VideoCodec::H264};
    bool enableAudio{true};
    bool enableInput{true};
    bool enableHdr{false};
};

struct GuestClientStats {
    uint64_t videoPacketsReceived{0};
    uint64_t framesDecoded{0};
    uint64_t framesPresented{0};
    uint64_t framesDropped{0};
    uint64_t audioPacketsReceived{0};
    uint64_t inputPacketsSent{0};
    double averageDecodeLatencyMs{0.0};
    double averageNetworkRttMs{0.0};
    double packetLossRate{0.0};
    double currentFps{0.0};
    std::string activeCodec{"H.264"};
    std::string platformName{"Windows (D3D11VA)"};
    SessionLifecycleState lifecycleState{SessionLifecycleState::Disconnected};
};

using FrameDecodedCallback = std::function<void(const DecodedFrame&)>;
using ConnectionStateCallback = std::function<void(bool isConnected)>;
using LifecycleStateCallback = std::function<void(SessionLifecycleState oldState, SessionLifecycleState newState)>;
using InputPacketOutboundCallback = std::function<void(const std::vector<uint8_t>& packetBytes)>;

class GuestClientSession {
public:
    explicit GuestClientSession(GuestClientConfig config) noexcept;
    ~GuestClientSession();

    [[nodiscard]] static common::Result<std::unique_ptr<GuestClientSession>> create(
        const GuestClientConfig& config
    );

    [[nodiscard]] common::Result<void> connect();
    [[nodiscard]] common::Result<void> disconnect();
    [[nodiscard]] bool isConnected() const noexcept { return isConnected_.load(); }
    [[nodiscard]] SessionLifecycleState getLifecycleState() const noexcept { return lifecycleState_.load(); }

    [[nodiscard]] common::Result<void> submitPairingCode(const std::string& code);
    [[nodiscard]] common::Result<void> authenticate(const std::string& authToken);
    [[nodiscard]] common::Result<void> negotiatePermissions(uint8_t requestedFeatures);

    [[nodiscard]] common::Result<void> setRelativeMouse(bool enabled);
    [[nodiscard]] bool isRelativeMouseEnabled() const noexcept { return isRelativeMouseEnabled_.load(); }

    [[nodiscard]] common::Result<void> triggerReconnect();

    [[nodiscard]] common::Result<void> ingestVideoPacket(const encode::EncodedPacket& packet);
    [[nodiscard]] common::Result<void> ingestAudioPacket(const uint8_t* data, size_t size, uint64_t timestampNs);

    [[nodiscard]] common::Result<void> sendInputEvent(const input::InputEvent& event);
    [[nodiscard]] common::Result<void> sendMouseMove(int32_t dx, int32_t dy, bool isAbsolute = false, float normX = 0.0f, float normY = 0.0f);
    [[nodiscard]] common::Result<void> sendMouseButton(input::MouseButton button, bool isDown);
    [[nodiscard]] common::Result<void> sendKeyboard(uint32_t vkCode, input::KeyAction action, uint32_t scanCode = 0);
    [[nodiscard]] common::Result<void> sendGamepad(const input::GamepadState& state);
    [[nodiscard]] common::Result<void> sendClipboard(const std::string& text);

    void setFrameDecodedCallback(FrameDecodedCallback callback);
    void setConnectionStateCallback(ConnectionStateCallback callback);
    void setLifecycleCallback(LifecycleStateCallback callback);
    void setInputOutboundCallback(InputPacketOutboundCallback callback);

    [[nodiscard]] GuestClientStats getStats() const;
    [[nodiscard]] const GuestClientConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] IVideoDecoder* getDecoder() const noexcept { return decoder_.get(); }
    [[nodiscard]] ClientPlatform getPlatform() const noexcept { return config_.platform; }

    [[nodiscard]] static std::vector<std::string> getPlatformRestrictions(ClientPlatform platform);

private:
    void transitionState(SessionLifecycleState newState);

    GuestClientConfig config_;
    std::unique_ptr<IVideoDecoder> decoder_;
    std::atomic<bool> isConnected_{false};
    std::atomic<bool> isRelativeMouseEnabled_{false};
    std::atomic<SessionLifecycleState> lifecycleState_{SessionLifecycleState::Disconnected};
    std::atomic<uint32_t> inputSequence_{0};
    uint8_t grantedPermissions_{0x07};

    mutable std::mutex statsMutex_;
    GuestClientStats stats_{};

    mutable std::mutex callbackMutex_;
    FrameDecodedCallback frameCallback_;
    ConnectionStateCallback connectionCallback_;
    LifecycleStateCallback lifecycleCallback_;
    InputPacketOutboundCallback inputCallback_;

    std::chrono::steady_clock::time_point lastFpsCalcTime_{};
    uint64_t lastFrameCount_{0};
};

} // namespace mist::decode
