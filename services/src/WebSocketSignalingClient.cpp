#include "mist/services/WebSocketSignalingClient.h"
#include "mist/common/Logger.h"
#include "rtc/rtc.hpp"
#include <sstream>

namespace mist::services {

WebSocketSignalingClient::WebSocketSignalingClient() noexcept = default;

WebSocketSignalingClient::~WebSocketSignalingClient() {
    (void)disconnect();
}

void WebSocketSignalingClient::setSessionInfo(const std::string& sessionId, const std::string& role) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    sessionId_ = sessionId;
    role_ = role;
}

common::Result<void> WebSocketSignalingClient::connect(
    const std::string& signalingServerUrl,
    const security::AuthToken& token
) {
    std::lock_guard<std::mutex> lock(mutex_);
    authToken_ = token;

    try {
        rtc::WebSocket::Configuration wsConfig{};
        ws_ = std::make_shared<rtc::WebSocket>(wsConfig);

        ws_->onOpen([this]() {
            std::vector<SignalingMessage> pending;
            {
                std::lock_guard<std::mutex> innerLock(mutex_);
                isConnected_.store(true);
                MIST_LOG_INFO(common::SubsystemId::Signaling, "Connected to WebSocket signaling server");

                // Register session
                std::stringstream ss;
                ss << "{\"type\":\"register\",\"session_id\":\"" << sessionId_
                   << "\",\"role\":\"" << role_
                   << "\",\"token\":\"" << authToken_.rawToken << "\"}";
                if (ws_) {
                    ws_->send(ss.str());
                }
                pending = std::move(pendingOutMessages_);
                pendingOutMessages_.clear();
            }

            for (const auto& msg : pending) {
                (void)sendMessage(msg);
            }
        });

        ws_->onClosed([this]() {
            isConnected_.store(false);
            MIST_LOG_INFO(common::SubsystemId::Signaling, "WebSocket signaling connection closed");
        });

        ws_->onError([this](std::string error) {
            MIST_LOG_ERROR(common::SubsystemId::Signaling, "WebSocket signaling error: " + error);
        });

        ws_->onMessage([this](rtc::message_variant data) {
            if (std::holds_alternative<std::string>(data)) {
                handleIncomingMessage(std::get<std::string>(data));
            }
        });

        ws_->open(signalingServerUrl);
        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Signaling, common::ErrorCode::NetworkError,
                             "Failed to connect to signaling server: " + std::string(e.what()));
    }
}

void WebSocketSignalingClient::handleIncomingMessage(const std::string& text) {
    MIST_LOG_DEBUG(common::SubsystemId::Signaling, "Signaling message received: " + text);

    // Parse simple JSON fields without heavyweight external JSON dependencies
    auto findField = [&text](std::string_view key) -> std::string {
        std::string needle = "\"" + std::string(key) + "\":\"";
        auto pos = text.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        auto endPos = text.find('"', pos);
        if (endPos == std::string::npos) return "";
        return text.substr(pos, endPos - pos);
    };

    std::string type = findField("type");
    if (type.empty()) return;

    SignalingMessage msg{};
    if (type == "offer") {
        msg.type = SignalingMessageType::Offer;
        // Extract raw SDP
        auto sdpPos = text.find("\"sdp\":\"");
        if (sdpPos != std::string::npos) {
            sdpPos += 7;
            auto sdpEnd = text.find("\"}", sdpPos);
            if (sdpEnd == std::string::npos) sdpEnd = text.rfind('"');
            msg.payloadJson = text.substr(sdpPos, sdpEnd - sdpPos);
            // Replace escaped newlines if any
            size_t nPos = 0;
            while ((nPos = msg.payloadJson.find("\\r\\n", nPos)) != std::string::npos) {
                msg.payloadJson.replace(nPos, 4, "\r\n");
                nPos += 2;
            }
        }
    } else if (type == "answer") {
        msg.type = SignalingMessageType::Answer;
        auto sdpPos = text.find("\"sdp\":\"");
        if (sdpPos != std::string::npos) {
            sdpPos += 7;
            auto sdpEnd = text.find("\"}", sdpPos);
            if (sdpEnd == std::string::npos) sdpEnd = text.rfind('"');
            msg.payloadJson = text.substr(sdpPos, sdpEnd - sdpPos);
            size_t nPos = 0;
            while ((nPos = msg.payloadJson.find("\\r\\n", nPos)) != std::string::npos) {
                msg.payloadJson.replace(nPos, 4, "\r\n");
                nPos += 2;
            }
        }
    } else if (type == "candidate") {
        msg.type = SignalingMessageType::IceCandidate;
        std::string cand = findField("candidate");
        std::string mid = findField("mid");
        msg.payloadJson = cand + "|" + mid;
    } else if (type == "peer_left" || type == "bye") {
        msg.type = SignalingMessageType::Disconnect;
    } else if (type == "peer_joined") {
        MIST_LOG_INFO(common::SubsystemId::Signaling, "Remote peer joined session");
        msg.type = SignalingMessageType::PeerJoined;
    } else if (type == "registered") {
        MIST_LOG_INFO(common::SubsystemId::Signaling, "Session successfully registered with signaling server");
        msg.type = SignalingMessageType::Registered;
        msg.payloadJson = (text.find("\"peer_connected\":true") != std::string::npos) ? "connected" : "alone";
    }

    if (callback_) {
        callback_(msg);
    }
}

common::Result<void> WebSocketSignalingClient::sendMessage(const SignalingMessage& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ws_ || !isConnected_.load()) {
        pendingOutMessages_.push_back(message);
        return common::Result<void>::success();
    }

    std::stringstream ss;
    switch (message.type) {
        case SignalingMessageType::Offer: {
            // Escape newlines in SDP
            std::string escapedSdp = message.payloadJson;
            size_t pos = 0;
            while ((pos = escapedSdp.find("\r\n", pos)) != std::string::npos) {
                escapedSdp.replace(pos, 2, "\\r\\n");
                pos += 4;
            }
            ss << "{\"type\":\"offer\",\"session_id\":\"" << sessionId_ << "\",\"sdp\":\"" << escapedSdp << "\"}";
            break;
        }
        case SignalingMessageType::Answer: {
            std::string escapedSdp = message.payloadJson;
            size_t pos = 0;
            while ((pos = escapedSdp.find("\r\n", pos)) != std::string::npos) {
                escapedSdp.replace(pos, 2, "\\r\\n");
                pos += 4;
            }
            ss << "{\"type\":\"answer\",\"session_id\":\"" << sessionId_ << "\",\"sdp\":\"" << escapedSdp << "\"}";
            break;
        }
        case SignalingMessageType::IceCandidate: {
            auto pipePos = message.payloadJson.find('|');
            std::string cand = (pipePos != std::string::npos) ? message.payloadJson.substr(0, pipePos) : message.payloadJson;
            std::string mid = (pipePos != std::string::npos) ? message.payloadJson.substr(pipePos + 1) : "video";
            ss << "{\"type\":\"candidate\",\"session_id\":\"" << sessionId_
               << "\",\"candidate\":\"" << cand << "\",\"mid\":\"" << mid << "\"}";
            break;
        }
        case SignalingMessageType::Disconnect:
            ss << "{\"type\":\"bye\",\"session_id\":\"" << sessionId_ << "\"}";
            break;
        case SignalingMessageType::PeerJoined:
        case SignalingMessageType::Registered:
            break;
    }

    try {
        ws_->send(ss.str());
        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Signaling, common::ErrorCode::InternalError,
                             "Failed to send signaling message: " + std::string(e.what()));
    }
}

common::Result<void> WebSocketSignalingClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ws_) {
        try {
            ws_->close();
        } catch (...) {}
        ws_.reset();
    }
    isConnected_.store(false);
    pendingOutMessages_.clear();
    return common::Result<void>::success();
}

void WebSocketSignalingClient::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

bool WebSocketSignalingClient::isConnected() const noexcept {
    return isConnected_.load();
}

} // namespace mist::services
