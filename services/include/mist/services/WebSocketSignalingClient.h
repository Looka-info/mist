#pragma once

#include "mist/services/ISignalingClient.h"
#include <memory>
#include <mutex>
#include <atomic>

namespace rtc {
class WebSocket;
}

namespace mist::services {

class WebSocketSignalingClient : public ISignalingClient {
public:
    WebSocketSignalingClient() noexcept;
    ~WebSocketSignalingClient() override;

    [[nodiscard]] common::Result<void> connect(
        const std::string& signalingServerUrl,
        const security::AuthToken& token
    ) override;

    [[nodiscard]] common::Result<void> sendMessage(const SignalingMessage& message) override;
    [[nodiscard]] common::Result<void> disconnect() override;
    void setMessageCallback(MessageCallback callback) override;
    [[nodiscard]] bool isConnected() const noexcept override;

    void setSessionInfo(const std::string& sessionId, const std::string& role) noexcept;
    [[nodiscard]] const std::string& getSessionId() const noexcept { return sessionId_; }
    [[nodiscard]] const std::string& getRole() const noexcept { return role_; }

private:
    void handleIncomingMessage(const std::string& text);

    mutable std::mutex mutex_;
    std::shared_ptr<rtc::WebSocket> ws_;
    MessageCallback callback_;
    std::string sessionId_{"mist-session-default"};
    std::string role_{"host"};
    security::AuthToken authToken_;
    std::atomic<bool> isConnected_{false};
    std::vector<SignalingMessage> pendingOutMessages_;
};

} // namespace mist::services
