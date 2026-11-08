#pragma once

#include "mist/common/Result.h"
#include "mist/security/Security.h"
#include <string>
#include <functional>

namespace mist::services {

enum class SignalingMessageType : uint8_t {
    Offer = 0,
    Answer,
    IceCandidate,
    Disconnect,
    PeerJoined,
    Registered
};

struct SignalingMessage {
    SignalingMessageType type{SignalingMessageType::Offer};
    std::string senderId;
    std::string targetId;
    std::string payloadJson;
};

using MessageCallback = std::function<void(const SignalingMessage& message)>;

class ISignalingClient {
public:
    virtual ~ISignalingClient() = default;

    [[nodiscard]] virtual common::Result<void> connect(const std::string& signalingServerUrl, const security::AuthToken& token) = 0;
    [[nodiscard]] virtual common::Result<void> sendMessage(const SignalingMessage& message) = 0;
    [[nodiscard]] virtual common::Result<void> disconnect() = 0;
    virtual void setMessageCallback(MessageCallback callback) = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
};

} // namespace mist::services
