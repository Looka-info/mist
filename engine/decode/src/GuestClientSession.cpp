#include "mist/decode/GuestClientSession.h"
#include "mist/common/Logger.h"
#include <cstring>

namespace mist::decode {

GuestClientSession::GuestClientSession(GuestClientConfig config) noexcept
    : config_(std::move(config)) {
    stats_.platformName = std::string(clientPlatformToString(config_.platform));
    stats_.activeCodec = config_.preferredCodec == encode::VideoCodec::AV1 ? "AV1" :
                         config_.preferredCodec == encode::VideoCodec::HEVC ? "HEVC" : "H.264";
    stats_.lifecycleState = SessionLifecycleState::Disconnected;
    lastFpsCalcTime_ = std::chrono::steady_clock::now();
}

GuestClientSession::~GuestClientSession() {
    (void)disconnect();
}

common::Result<std::unique_ptr<GuestClientSession>> GuestClientSession::create(
    const GuestClientConfig& config
) {
    auto session = std::make_unique<GuestClientSession>(config);
    auto decRes = VideoDecoderFactory::createDecoder(config.platform);
    if (!decRes.hasValue()) {
        return decRes.error();
    }
    session->decoder_ = std::move(decRes.value());
    return session;
}

void GuestClientSession::transitionState(SessionLifecycleState newState) {
    SessionLifecycleState oldState = lifecycleState_.exchange(newState);
    if (oldState != newState) {
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.lifecycleState = newState;
        }

        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (lifecycleCallback_) {
            lifecycleCallback_(oldState, newState);
        }
    }
}

common::Result<void> GuestClientSession::submitPairingCode(const std::string& code) {
    if (code.size() < 6) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::InvalidArgument,
                             "Pairing code must be at least 6 characters");
    }

    transitionState(SessionLifecycleState::Pairing);
    MIST_LOG_INFO(common::SubsystemId::Session, "Guest submitted pairing code to host");
    transitionState(SessionLifecycleState::Authenticating);
    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::authenticate(const std::string& authToken) {
    if (authToken.empty()) {
        transitionState(SessionLifecycleState::Failed);
        return common::Error(common::SubsystemId::Security, common::ErrorCode::AuthenticationFailed,
                             "Empty authentication token");
    }

    transitionState(SessionLifecycleState::Authenticating);
    config_.authToken = authToken;
    transitionState(SessionLifecycleState::Signaling);
    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::negotiatePermissions(uint8_t requestedFeatures) {
    grantedPermissions_ = requestedFeatures;

    if (isConnected_.load()) {
        input::PermissionMessage msg{
            .action = input::PermissionAction::Request,
            .requestedFeatures = requestedFeatures
        };
        return sendInputEvent(msg);
    }
    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::setRelativeMouse(bool enabled) {
    isRelativeMouseEnabled_.store(enabled);
    MIST_LOG_INFO(common::SubsystemId::Input,
                  std::string("Guest relative mouse mode ") + (enabled ? "enabled" : "disabled"));
    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::triggerReconnect() {
    MIST_LOG_INFO(common::SubsystemId::Session, "Triggering guest session reconnection...");
    transitionState(SessionLifecycleState::Reconnecting);

    if (decoder_) {
        (void)decoder_->flush();
    }

    isConnected_.store(false);
    return connect();
}

common::Result<void> GuestClientSession::connect() {
    if (isConnected_.load()) {
        return common::Result<void>::success();
    }

    if (!decoder_) {
        transitionState(SessionLifecycleState::Failed);
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidState,
                             "Decoder not initialized in GuestClientSession");
    }

    transitionState(SessionLifecycleState::ConnectingIce);

    DecodeConfig decConfig{
        .codec = config_.preferredCodec,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    auto initRes = decoder_->initialize(decConfig);
    if (!initRes.hasValue()) {
        transitionState(SessionLifecycleState::Failed);
        return initRes.error();
    }

    isConnected_.store(true);
    transitionState(SessionLifecycleState::Connected);

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (connectionCallback_) {
            connectionCallback_(true);
        }
    }

    MIST_LOG_INFO(common::SubsystemId::Decode,
                  "GuestClientSession connected on platform " + std::string(clientPlatformToString(config_.platform)));
    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::disconnect() {
    if (!isConnected_.load() && lifecycleState_.load() == SessionLifecycleState::Disconnected) {
        return common::Result<void>::success();
    }

    isConnected_.store(false);
    isRelativeMouseEnabled_.store(false);
    transitionState(SessionLifecycleState::Disconnected);

    if (decoder_) {
        (void)decoder_->flush();
        (void)decoder_->shutdown();
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (connectionCallback_) {
            connectionCallback_(false);
        }
    }

    MIST_LOG_INFO(common::SubsystemId::Decode, "GuestClientSession disconnected");
    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::ingestVideoPacket(const encode::EncodedPacket& packet) {
    if (!isConnected_.load()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidState,
                             "GuestClientSession is not connected");
    }
    if (!decoder_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidState,
                             "No video decoder available");
    }

    if (lifecycleState_.load() == SessionLifecycleState::Connected) {
        transitionState(SessionLifecycleState::Streaming);
    }

    auto startT = std::chrono::steady_clock::now();
    auto subRes = decoder_->submitPacket(packet);
    if (!subRes.hasValue()) {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.framesDropped;
        return subRes.error();
    }

    auto getRes = decoder_->getNextFrame();
    auto endT = std::chrono::steady_clock::now();
    double latencyMs = std::chrono::duration<double, std::milli>(endT - startT).count();

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.videoPacketsReceived;
        if (stats_.averageDecodeLatencyMs <= 0.0) {
            stats_.averageDecodeLatencyMs = latencyMs;
        } else {
            stats_.averageDecodeLatencyMs = stats_.averageDecodeLatencyMs * 0.95 + latencyMs * 0.05;
        }
    }

    if (getRes.hasValue()) {
        DecodedFrame frame = getRes.value();
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            ++stats_.framesDecoded;
            ++stats_.framesPresented;
        }

        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (frameCallback_) {
                frameCallback_(frame);
            }
        }

        (void)decoder_->releaseFrame(frame);
    }

    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::ingestAudioPacket(const uint8_t* data, size_t size, uint64_t /*timestampNs*/) {
    if (!isConnected_.load()) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidState,
                             "GuestClientSession is not connected");
    }
    if (!config_.enableAudio || !data || size == 0) {
        return common::Result<void>::success();
    }

    std::lock_guard<std::mutex> lock(statsMutex_);
    ++stats_.audioPacketsReceived;
    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::sendInputEvent(const input::InputEvent& event) {
    if (!isConnected_.load()) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidState,
                             "GuestClientSession is not connected");
    }
    if (!config_.enableInput) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::PermissionDenied,
                             "Input is disabled for this guest session");
    }

    input::InputPacketHeader header{};
    header.version = input::INPUT_PROTOCOL_VERSION;
    header.sequenceNumber = ++inputSequence_;
    header.timestampNs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
    std::strncpy(header.sessionId, config_.sessionId.c_str(), sizeof(header.sessionId) - 1);

    std::vector<uint8_t> payload;
    std::visit([&](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, input::KeyEvent>) {
            payload = input::InputProtocol::serializeKeyEvent(header, e);
        } else if constexpr (std::is_same_v<T, input::MouseMoveEvent>) {
            payload = input::InputProtocol::serializeMouseMoveEvent(header, e);
        } else if constexpr (std::is_same_v<T, input::MouseButtonEvent>) {
            payload = input::InputProtocol::serializeMouseButtonEvent(header, e);
        } else if constexpr (std::is_same_v<T, input::MouseWheelEvent>) {
            payload = input::InputProtocol::serializeMouseWheelEvent(header, e);
        } else if constexpr (std::is_same_v<T, input::GamepadState>) {
            payload = input::InputProtocol::serializeGamepadState(header, e);
        } else if constexpr (std::is_same_v<T, input::PermissionMessage>) {
            payload = input::InputProtocol::serializePermissionMessage(header, e);
        } else if constexpr (std::is_same_v<T, input::ClipboardEvent>) {
            payload = input::InputProtocol::serializeClipboardEvent(header, e.text);
        }
    }, event);

    if (payload.empty()) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Failed to serialize input event");
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (inputCallback_) {
            inputCallback_(payload);
        }
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ++stats_.inputPacketsSent;
    }

    return common::Result<void>::success();
}

common::Result<void> GuestClientSession::sendMouseMove(int32_t dx, int32_t dy, bool isAbsolute, float normX, float normY) {
    input::MouseMoveEvent e{
        .deltaX = dx,
        .deltaY = dy,
        .isAbsolute = isAbsolute,
        .normalizedX = normX,
        .normalizedY = normY
    };
    return sendInputEvent(e);
}

common::Result<void> GuestClientSession::sendMouseButton(input::MouseButton button, bool isDown) {
    input::MouseButtonEvent e{
        .button = button,
        .isDown = isDown
    };
    return sendInputEvent(e);
}

common::Result<void> GuestClientSession::sendKeyboard(uint32_t vkCode, input::KeyAction action, uint32_t scanCode) {
    input::KeyEvent e{
        .virtualKey = vkCode,
        .scanCode = scanCode,
        .action = action,
        .isExtended = false
    };
    return sendInputEvent(e);
}

common::Result<void> GuestClientSession::sendGamepad(const input::GamepadState& state) {
    return sendInputEvent(state);
}

common::Result<void> GuestClientSession::sendClipboard(const std::string& text) {
    input::ClipboardEvent e{
        .text = text,
        .hash = 0
    };
    return sendInputEvent(e);
}

void GuestClientSession::setFrameDecodedCallback(FrameDecodedCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = std::move(callback);
}

void GuestClientSession::setConnectionStateCallback(ConnectionStateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    connectionCallback_ = std::move(callback);
}

void GuestClientSession::setLifecycleCallback(LifecycleStateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    lifecycleCallback_ = std::move(callback);
}

void GuestClientSession::setInputOutboundCallback(InputPacketOutboundCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    inputCallback_ = std::move(callback);
}

GuestClientStats GuestClientSession::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    GuestClientStats s = stats_;

    auto now = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(now - lastFpsCalcTime_).count();
    if (elapsedSec >= 1.0) {
        uint64_t diff = s.framesPresented - lastFrameCount_;
        const_cast<GuestClientSession*>(this)->lastFrameCount_ = s.framesPresented;
        const_cast<GuestClientSession*>(this)->lastFpsCalcTime_ = now;
        s.currentFps = static_cast<double>(diff) / elapsedSec;
    } else {
        s.currentFps = s.framesPresented > 0 ? static_cast<double>(config_.targetFps) : 0.0;
    }

    return s;
}

std::vector<std::string> GuestClientSession::getPlatformRestrictions(ClientPlatform platform) {
    switch (platform) {
        case ClientPlatform::Windows_D3D11VA:
            return {
                "Requires Direct3D 11 feature level 11_0+ for hardware decoding",
                "WGC borderless capture requires Windows 10 build 19041 or higher",
                "Virtual gamepad driver requires ViGEmBus kernel service installed on host"
            };
        case ClientPlatform::MacOS_VideoToolbox:
            return {
                "macOS accessibility permissions required in System Settings for remote input injection",
                "Screen Recording permission required in Security & Privacy for host screen capture",
                "Clipboard paste synchronization requires active application foreground focus"
            };
        case ClientPlatform::Linux_VaApi:
            return {
                "VA-API hardware decode requires user membership in 'render' or 'video' Unix group",
                "Wayland compositors require xdg-desktop-portal / PipeWire for remote screen capture",
                "Zero-copy DMA-BUF texture sharing requires EGL extension EGL_EXT_image_dma_buf_import"
            };
        case ClientPlatform::Android_MediaCodec:
            return {
                "ANativeWindow surface must be provided and managed by Android Activity lifecycle",
                "Touch events require software translation to relative mouse delta events",
                "Background streaming paused when Activity enters onPause/onStop lifecycle"
            };
        case ClientPlatform::Web_WebRtcWasm:
            return {
                "WebCodecs API requires secure browsing context (HTTPS or localhost)",
                "Relative mouse lock (Pointer Lock API) requires explicit user gesture click",
                "Clipboard write requires browser 'clipboard-write' permission grant"
            };
    }
    return {};
}

} // namespace mist::decode
