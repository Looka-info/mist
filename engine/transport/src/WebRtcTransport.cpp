#include "mist/transport/WebRtcTransport.h"
#include "mist/common/Logger.h"
#include "mist/services/WebSocketSignalingClient.h"
#include "rtc/rtc.hpp"

namespace mist::transport {

WebRtcTransport::WebRtcTransport() noexcept
    : stats_(std::make_shared<TransportStatistics>()) {}

WebRtcTransport::~WebRtcTransport() {
    (void)disconnect();
}

std::unique_ptr<WebRtcTransport> WebRtcTransport::create() noexcept {
    return std::make_unique<WebRtcTransport>();
}

common::Result<void> WebRtcTransport::initialize(const TransportConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    genericConfig_ = config;

    WebRtcTransportConfig rtcConf{};
    rtcConf.isHost = (config.remoteAddress.empty() || config.remoteAddress == "127.0.0.1" || config.remoteAddress == "0.0.0.0");
    rtcConf.signalingUrl = "ws://" + (config.remoteAddress.empty() ? "127.0.0.1" : config.remoteAddress) + ":8080";

    return initializeWebRtc(rtcConf);
}

common::Result<void> WebRtcTransport::initializeWebRtc(
    const WebRtcTransportConfig& config,
    std::shared_ptr<services::ISignalingClient> signalingClient
) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    signalingClient_ = std::move(signalingClient);

    pcManager_ = std::make_shared<PeerConnectionManager>();
    auto pcRes = pcManager_->initialize(config_);
    if (!pcRes) {
        return pcRes;
    }

    auto pc = pcManager_->getPeerConnection();
    if (!pc) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError, "Failed to acquire PeerConnection");
    }

    if (config_.isHost) {
        videoSender_ = std::make_shared<VideoTrackSender>();
        auto vRes = videoSender_->initialize(pc, config_);
        if (!vRes) return vRes;

        qosController_ = std::make_shared<qos::AdaptiveBitrateController>();
        videoSender_->setRembReceivedCallback([this](uint32_t bitrateBps) {
            if (qosController_) {
                qosController_->onRembReceived(bitrateBps);
            }
        });

        if (config_.enableAudio) {
            audioSender_ = std::make_shared<AudioTrackSender>();
            auto aRes = audioSender_->initialize(pc, config_);
            if (!aRes) return aRes;
        }

        dcManager_ = std::make_shared<DataChannelManager>();
        auto dcRes = dcManager_->initialize(pc, /*isHost=*/true);
        if (!dcRes) return dcRes;
    } else {
        videoReceiver_ = std::make_shared<VideoTrackReceiver>();
        audioReceiver_ = std::make_shared<AudioTrackReceiver>();
        (void)audioReceiver_->initialize(config_);
        if (audioFrameCb_) {
            audioReceiver_->setFrameCallback(audioFrameCb_);
        }

        // Multiplex incoming remote tracks between video and audio
        pc->onTrack([this](std::shared_ptr<rtc::Track> track) {
            if (!track) return;
            if (track->description().type() == "audio") {
                if (audioReceiver_) {
                    audioReceiver_->attachTrack(track);
                }
            } else {
                if (videoReceiver_) {
                    videoReceiver_->attachTrack(track);
                }
            }
        });

        dcManager_ = std::make_shared<DataChannelManager>();
        auto dcRes = dcManager_->initialize(pc, /*isHost=*/false);
        if (!dcRes) return dcRes;
    }

    if (dcManager_) {
        dcManager_->setMessageCallback([this](const std::string& label, std::span<const uint8_t> data) {
            if (label == "input-events" || label == "input-state") {
                if (inputDataCb_) {
                    inputDataCb_(label, data);
                }
            } else if (genericDataCb_) {
                genericDataCb_(TransportChannel::Control, data);
            }
        });
    }

    if (signalingClient_) {
        auto wsClient = std::dynamic_pointer_cast<services::WebSocketSignalingClient>(signalingClient_);
        if (wsClient) {
            wsClient->setSessionInfo(config_.sessionId, config_.isHost ? "host" : "client");
        }
    }

    identityVerifier_.setExpectedRemoteFingerprint(config_.expectedRemoteFingerprint);
    identityVerifier_.setSessionContext(config_.sessionId, config_.authToken);

    setupSignalingRouting();

    isInitialized_ = true;
    MIST_LOG_INFO(common::SubsystemId::Transport, "WebRtcTransport initialized (" +
                  std::string(config_.isHost ? "Host" : "Client") + ")");
    return common::Result<void>::success();
}

void WebRtcTransport::setupSignalingRouting() {
    if (!pcManager_) return;

    if (signalingClient_) {
        pcManager_->setLocalDescriptionCallback([this](const std::string& type, const std::string& sdp) {
            identityVerifier_.setLocalFingerprint(security::PeerIdentityVerifier::extractFingerprintFromSdp(sdp));
            services::SignalingMessage msg{};
            msg.type = (type == "offer") ? services::SignalingMessageType::Offer : services::SignalingMessageType::Answer;
            msg.payloadJson = sdp;
            (void)signalingClient_->sendMessage(msg);
        });

        pcManager_->setLocalCandidateCallback([this](const std::string& candidate, const std::string& mid) {
            services::SignalingMessage msg{};
            msg.type = services::SignalingMessageType::IceCandidate;
            msg.payloadJson = candidate + "|" + mid;
            (void)signalingClient_->sendMessage(msg);
        });
    }

    pcManager_->setStateChangeCallback([this](WebRtcState st) {
        if (stats_) {
            switch (st) {
                case WebRtcState::Connecting: stats_->setTransportState("connecting"); break;
                case WebRtcState::Connected: stats_->setTransportState("connected"); break;
                case WebRtcState::Disconnected: stats_->setTransportState("disconnected"); break;
                case WebRtcState::Failed: stats_->setTransportState("failed"); break;
                case WebRtcState::Closed: stats_->setTransportState("closed"); break;
                default: stats_->setTransportState("new"); break;
            }
        }
    });

    if (signalingClient_) {
        signalingClient_->setMessageCallback([this](const services::SignalingMessage& msg) {
            if (!pcManager_) return;
            switch (msg.type) {
                case services::SignalingMessageType::Offer:
                    if (auto vRes = identityVerifier_.verifyRemoteDescription(msg.payloadJson); !vRes.hasValue()) {
                        MIST_LOG_ERROR(common::SubsystemId::Transport, "Offer identity verification failed: " + vRes.error().message());
                        break;
                    }
                    (void)pcManager_->setRemoteDescription("offer", msg.payloadJson);
                    break;
                case services::SignalingMessageType::Answer:
                    if (auto vRes = identityVerifier_.verifyRemoteDescription(msg.payloadJson); !vRes.hasValue()) {
                        MIST_LOG_ERROR(common::SubsystemId::Transport, "Answer identity verification failed: " + vRes.error().message());
                        break;
                    }
                    (void)pcManager_->setRemoteDescription("answer", msg.payloadJson);
                    break;
                case services::SignalingMessageType::IceCandidate: {
                    auto pipePos = msg.payloadJson.find('|');
                    std::string cand = (pipePos != std::string::npos) ? msg.payloadJson.substr(0, pipePos) : msg.payloadJson;
                    std::string mid = (pipePos != std::string::npos) ? msg.payloadJson.substr(pipePos + 1) : "video";
                    (void)pcManager_->addRemoteCandidate(cand, mid);
                    break;
                }
                case services::SignalingMessageType::PeerJoined:
                    if (config_.isHost) {
                        MIST_LOG_INFO(common::SubsystemId::Transport, "PeerJoined received on host; creating offer");
                        (void)pcManager_->createOffer();
                    }
                    break;
                case services::SignalingMessageType::Registered:
                    if (config_.isHost && msg.payloadJson == "connected") {
                        MIST_LOG_INFO(common::SubsystemId::Transport, "Host registered with peer already in session; creating offer");
                        (void)pcManager_->createOffer();
                    }
                    break;
                case services::SignalingMessageType::Disconnect:
                    (void)pcManager_->close();
                    break;
            }
        });
    }
}

common::Result<void> WebRtcTransport::createOffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pcManager_) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "PeerConnectionManager not initialized");
    }
    return pcManager_->createOffer();
}

common::Result<void> WebRtcTransport::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "WebRtcTransport not initialized");
    }

    if (signalingClient_) {
        security::AuthToken token{config_.authToken.empty() ? "test-token" : config_.authToken, 3600};
        auto res = signalingClient_->connect(config_.signalingUrl, token);
        if (!res) {
            return res;
        }
    }

    return common::Result<void>::success();
}

common::Result<void> WebRtcTransport::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (signalingClient_) {
        (void)signalingClient_->disconnect();
    }
    if (videoSender_) {
        videoSender_->close();
    }
    if (videoReceiver_) {
        videoReceiver_->close();
    }
    if (audioSender_) {
        audioSender_->close();
    }
    if (audioReceiver_) {
        audioReceiver_->close();
    }
    if (dcManager_) {
        dcManager_->close();
    }
    if (pcManager_) {
        (void)pcManager_->close();
    }
    if (stats_) {
        stats_->setTransportState("closed");
    }
    return common::Result<void>::success();
}

common::Result<void> WebRtcTransport::send(TransportChannel channel, std::span<const uint8_t> data) {
    if (channel == TransportChannel::Video) {
        encode::EncodedPacket packet{};
        packet.data.assign(data.begin(), data.end());
        return sendVideoPacket(packet);
    }

    if (channel == TransportChannel::Audio) {
        return sendAudioFrame(data, 480);
    }

    if (dcManager_) {
        std::string label = (channel == TransportChannel::Control) ? "session-control" : "diagnostics";
        return dcManager_->sendData(label, data);
    }

    return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidState, "Transport channel not ready");
}

common::Result<void> WebRtcTransport::sendVideoPacket(const encode::EncodedPacket& packet) {
    if (!videoSender_) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "Host video sender not initialized");
    }

    if (stats_) {
        stats_->recordPacketSent();
        stats_->recordBytesSent(packet.data.size());
    }

    return videoSender_->sendFrame(packet);
}

common::Result<void> WebRtcTransport::sendAudioFrame(std::span<const uint8_t> opusPayload, uint32_t sampleCount) {
    std::shared_ptr<AudioTrackSender> sender;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sender = audioSender_;
    }
    if (!sender) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "Host audio sender not initialized");
    }

    if (stats_) {
        stats_->recordPacketSent();
        stats_->recordBytesSent(opusPayload.size());
    }

    return sender->sendFrame(opusPayload, sampleCount);
}

void WebRtcTransport::setKeyframeRequestCallback(KeyframeRequestCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (videoSender_) {
        videoSender_->setKeyframeRequestCallback(std::move(cb));
    }
}

void WebRtcTransport::setFrameReceivedCallback(FrameReceivedCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (videoReceiver_) {
        videoReceiver_->setFrameReceivedCallback([this, cb = std::move(cb)](const std::vector<uint8_t>& accessUnit, uint64_t timestampNs) {
            if (stats_) {
                stats_->recordPacketReceived();
                stats_->recordBytesReceived(accessUnit.size());
            }
            cb(accessUnit, timestampNs);
        });
    }
}

void WebRtcTransport::setAudioFrameCallback(AudioFrameCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    audioFrameCb_ = std::move(cb);
    if (audioReceiver_) {
        audioReceiver_->setFrameCallback([this](std::span<const uint8_t> payload, const AudioFrameInfo& info) {
            if (stats_) {
                stats_->recordPacketReceived();
                stats_->recordBytesReceived(payload.size());
            }
            if (audioFrameCb_) {
                audioFrameCb_(payload, info);
            }
        });
    }
}

common::Result<void> WebRtcTransport::sendInputData(const std::string& label, std::span<const uint8_t> data) {
    if (!dcManager_) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "DataChannelManager is null");
    }
    return dcManager_->sendData(label, data);
}

void WebRtcTransport::setInputDataCallback(InputDataCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    inputDataCb_ = std::move(cb);
}

void WebRtcTransport::setDataReceivedCallback(DataReceivedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    genericDataCb_ = std::move(callback);
}

bool WebRtcTransport::isConnected() const noexcept {
    return pcManager_ && pcManager_->isConnected();
}

common::Result<void> WebRtcTransport::requestVideoBitrate(uint32_t bitrateBps) {
    std::shared_ptr<VideoTrackReceiver> vr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        vr = videoReceiver_;
    }
    if (vr) {
        vr->requestBitrate(bitrateBps);
        return common::Result<void>::success();
    }
    return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidState, "VideoTrackReceiver unavailable");
}

common::Result<void> WebRtcTransport::restartIce() {
    std::shared_ptr<PeerConnectionManager> pcm;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pcm = pcManager_;
    }
    if (pcm) {
        return pcm->restartIce();
    }
    return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidState, "PeerConnectionManager unavailable");
}

TransportStatsSnapshot WebRtcTransport::getStatsSnapshot() const noexcept {
    if (stats_) {
        return stats_->getSnapshot();
    }
    return TransportStatsSnapshot{};
}

NatDiagnostics WebRtcTransport::getNatDiagnostics() const noexcept {
    std::shared_ptr<PeerConnectionManager> pcm;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pcm = pcManager_;
    }
    if (pcm) {
        return pcm->getNatDiagnostics();
    }
    return NatDiagnostics{};
}

} // namespace mist::transport
