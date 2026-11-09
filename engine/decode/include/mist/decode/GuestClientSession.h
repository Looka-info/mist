#pragma once

#include "mist/decode/CrossPlatformDecoders.h"
#include "mist/decode/IVideoDecoder.h"
#include "mist/encode/IVideoEncoder.h"
#include "mist/input/InputTypes.h"
#include "mist/input/InputProtocol.h"
#include "mist/common/Result.h"
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>

namespace mist::decode {

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
};

using FrameDecodedCallback = std::function<void(const DecodedFrame&)>;
using ConnectionStateCallback = std::function<void(bool isConnected)>;
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
    void setInputOutboundCallback(InputPacketOutboundCallback callback);

    [[nodiscard]] GuestClientStats getStats() const;
    [[nodiscard]] const GuestClientConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] IVideoDecoder* getDecoder() const noexcept { return decoder_.get(); }
    [[nodiscard]] ClientPlatform getPlatform() const noexcept { return config_.platform; }

private:
    GuestClientConfig config_;
    std::unique_ptr<IVideoDecoder> decoder_;
    std::atomic<bool> isConnected_{false};
    std::atomic<uint32_t> inputSequence_{0};

    mutable std::mutex statsMutex_;
    GuestClientStats stats_{};

    mutable std::mutex callbackMutex_;
    FrameDecodedCallback frameCallback_;
    ConnectionStateCallback connectionCallback_;
    InputPacketOutboundCallback inputCallback_;

    std::chrono::steady_clock::time_point lastFpsCalcTime_{};
    uint64_t lastFrameCount_{0};
};

} // namespace mist::decode
