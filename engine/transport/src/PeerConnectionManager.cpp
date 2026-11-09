#include "mist/transport/PeerConnectionManager.h"
#include "mist/common/Logger.h"
#include "rtc/rtc.hpp"

namespace mist::transport {

PeerConnectionManager::PeerConnectionManager() noexcept = default;

PeerConnectionManager::~PeerConnectionManager() {
    (void)close();
}

common::Result<void> PeerConnectionManager::initialize(const WebRtcTransportConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    rtc::Configuration rtcConfig{};
    for (const auto& s : config_.iceServers) {
        if (!s.username.empty() || !s.credential.empty()) {
            std::string url = s.url;
            std::string hostPort = url;
            if (url.starts_with("turn:")) {
                hostPort = url.substr(5);
            } else if (url.starts_with("turns:")) {
                hostPort = url.substr(6);
            }
            // Strip any URI query params like ?transport=tcp
            auto qPos = hostPort.find('?');
            std::string query = (qPos != std::string::npos) ? hostPort.substr(qPos) : "";
            if (qPos != std::string::npos) {
                hostPort = hostPort.substr(0, qPos);
            }
            std::string host = hostPort;
            uint16_t port = 3478;
            auto colonPos = hostPort.find(':');
            if (colonPos != std::string::npos) {
                host = hostPort.substr(0, colonPos);
                try {
                    port = static_cast<uint16_t>(std::stoi(hostPort.substr(colonPos + 1)));
                } catch (...) {
                    port = 3478;
                }
            }
            rtc::IceServer::RelayType relayType = rtc::IceServer::RelayType::TurnUdp;
            if (query.find("transport=tcp") != std::string::npos) {
                relayType = rtc::IceServer::RelayType::TurnTcp;
            } else if (url.starts_with("turns:")) {
                relayType = rtc::IceServer::RelayType::TurnTls;
            }
            rtcConfig.iceServers.emplace_back(host, port, s.username, s.credential, relayType);
        } else {
            rtcConfig.iceServers.emplace_back(s.url);
        }
    }
    if (config_.minPort > 0 && config_.maxPort > config_.minPort) {
        rtcConfig.portRangeBegin = config_.minPort;
        rtcConfig.portRangeEnd = config_.maxPort;
    }

    try {
        pc_ = std::make_shared<rtc::PeerConnection>(rtcConfig);
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Failed to create PeerConnection: " + std::string(e.what()));
    }

    pc_->onLocalDescription([this](rtc::Description desc) {
        LocalDescriptionCallback cb;
        std::string typeStr = desc.typeString();
        std::string sdpStr = std::string(desc);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            MIST_LOG_INFO(common::SubsystemId::Transport, "Local description generated: " + typeStr);
            if (localDescCb_) {
                cb = localDescCb_;
            } else {
                pendingLocalDesc_ = {typeStr, sdpStr};
            }
        }
        if (cb) {
            cb(typeStr, sdpStr);
        }
    });

    pc_->onLocalCandidate([this](rtc::Candidate cand) {
        LocalCandidateCallback cb;
        std::string candStr = std::string(cand);
        std::string midStr = cand.mid();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            MIST_LOG_INFO(common::SubsystemId::Transport, "Local ICE candidate gathered (mid: " + midStr + "): " + candStr);
            if (auto parsed = IceCandidatePrioritizer::parseSdpCandidate(candStr)) {
                localParsedCandidates_.push_back(*parsed);
            }
            if (localCandidateCb_) {
                cb = localCandidateCb_;
            } else {
                pendingCandidates_.emplace_back(candStr, midStr);
            }
        }
        if (cb) {
            cb(candStr, midStr);
        }
    });

    pc_->onStateChange([this](rtc::PeerConnection::State state) {
        StateChangeCallback cb;
        WebRtcState st = WebRtcState::New;
        switch (state) {
            case rtc::PeerConnection::State::Connecting:
                st = WebRtcState::Connecting;
                MIST_LOG_INFO(common::SubsystemId::Transport, "PeerConnection State: Connecting");
                break;
            case rtc::PeerConnection::State::Connected:
                st = WebRtcState::Connected;
                MIST_LOG_INFO(common::SubsystemId::Transport, "PeerConnection State: Connected");
                break;
            case rtc::PeerConnection::State::Disconnected:
                st = WebRtcState::Disconnected;
                MIST_LOG_WARN(common::SubsystemId::Transport, "PeerConnection State: Disconnected");
                break;
            case rtc::PeerConnection::State::Failed:
                st = WebRtcState::Failed;
                MIST_LOG_ERROR(common::SubsystemId::Transport, "PeerConnection State: Failed");
                break;
            case rtc::PeerConnection::State::Closed:
                st = WebRtcState::Closed;
                MIST_LOG_INFO(common::SubsystemId::Transport, "PeerConnection State: Closed");
                break;
            default:
                break;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = st;
            cb = stateChangeCb_;
        }
        if (cb) {
            cb(st);
        }
    });

    MIST_LOG_INFO(common::SubsystemId::Transport, "PeerConnection initialized for session: " + config_.sessionId);
    return common::Result<void>::success();
}

void PeerConnectionManager::setLocalDescriptionCallback(LocalDescriptionCallback cb) {
    LocalDescriptionCallback toFire;
    std::pair<std::string, std::string> descToFire;
    bool hasDesc = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        localDescCb_ = std::move(cb);
        if (localDescCb_ && pendingLocalDesc_.has_value()) {
            toFire = localDescCb_;
            descToFire = *pendingLocalDesc_;
            pendingLocalDesc_.reset();
            hasDesc = true;
        }
    }

    if (hasDesc && toFire) {
        toFire(descToFire.first, descToFire.second);
    }
}

void PeerConnectionManager::setLocalCandidateCallback(LocalCandidateCallback cb) {
    LocalCandidateCallback toFire;
    std::vector<std::pair<std::string, std::string>> candsToFire;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        localCandidateCb_ = std::move(cb);
        if (localCandidateCb_ && !pendingCandidates_.empty()) {
            toFire = localCandidateCb_;
            candsToFire = std::move(pendingCandidates_);
            pendingCandidates_.clear();
        }
    }

    if (toFire) {
        for (const auto& [cand, mid] : candsToFire) {
            toFire(cand, mid);
        }
    }
}

void PeerConnectionManager::setStateChangeCallback(StateChangeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateChangeCb_ = std::move(cb);
}

common::Result<void> PeerConnectionManager::createOffer() {
    LocalDescriptionCallback cb;
    std::pair<std::string, std::string> descToFire;
    bool fireNow = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pc_) {
            return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "PeerConnection not initialized");
        }

        try {
            if (auto curDesc = pc_->localDescription(); curDesc.has_value() && curDesc->type() == rtc::Description::Type::Offer) {
                if (localDescCb_) {
                    cb = localDescCb_;
                    descToFire = {curDesc->typeString(), std::string(curDesc.value())};
                    fireNow = true;
                }
            } else {
                pc_->setLocalDescription(rtc::Description::Type::Offer);
                MIST_LOG_INFO(common::SubsystemId::Transport, "Initiated local offer creation");
            }
        } catch (const std::exception& e) {
            return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                                 "createOffer failed: " + std::string(e.what()));
        }
    }

    if (fireNow && cb) {
        cb(descToFire.first, descToFire.second);
    }
    return common::Result<void>::success();
}

common::Result<void> PeerConnectionManager::setRemoteDescription(const std::string& type, const std::string& sdp) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pc_) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "PeerConnection not initialized");
    }

    try {
        rtc::Description desc(sdp, type);
        pc_->setRemoteDescription(desc);
        hasRemoteDescription_ = true;
        MIST_LOG_INFO(common::SubsystemId::Transport, "Applied remote description (" + type + ")");

        // Flush any remote candidates received before remote description was set
        for (const auto& [cand, mid] : pendingRemoteCandidates_) {
            try {
                MIST_LOG_INFO(common::SubsystemId::Transport, "Flushing queued candidate (mid: " + mid + "): " + cand);
                rtc::Candidate rtcCand(cand);
                if (!mid.empty()) {
                    rtcCand.hintMid(mid);
                }
                pc_->addRemoteCandidate(rtcCand);
            } catch (const std::exception& e) {
                MIST_LOG_WARN(common::SubsystemId::Transport, "Failed to apply queued candidate: " + std::string(e.what()));
            }
        }
        pendingRemoteCandidates_.clear();

        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "setRemoteDescription failed: " + std::string(e.what()));
    }
}

common::Result<void> PeerConnectionManager::addRemoteCandidate(const std::string& candidate, const std::string& mid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pc_) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotInitialized, "PeerConnection not initialized");
    }

    if (!hasRemoteDescription_) {
        MIST_LOG_INFO(common::SubsystemId::Transport, "Queueing candidate before remote description (mid: " + mid + "): " + candidate);
        pendingRemoteCandidates_.emplace_back(candidate, mid);
        if (auto parsed = IceCandidatePrioritizer::parseSdpCandidate(candidate)) {
            remoteParsedCandidates_.push_back(*parsed);
        }
        return common::Result<void>::success();
    }

    try {
        MIST_LOG_INFO(common::SubsystemId::Transport, "Adding remote candidate (mid: " + mid + "): " + candidate);
        rtc::Candidate cand(candidate);
        if (!mid.empty()) {
            cand.hintMid(mid);
        }
        pc_->addRemoteCandidate(cand);
        if (auto parsed = IceCandidatePrioritizer::parseSdpCandidate(candidate)) {
            remoteParsedCandidates_.push_back(*parsed);
        }
        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "addRemoteCandidate failed: " + std::string(e.what()));
    }
}

common::Result<void> PeerConnectionManager::close() {
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = std::move(pc_);
        hasRemoteDescription_ = false;
        pendingRemoteCandidates_.clear();
        localParsedCandidates_.clear();
        remoteParsedCandidates_.clear();
        portMappingManager_.releaseAllMappings();
        state_ = WebRtcState::Closed;
    }
    if (pc) {
        try {
            pc->close();
        } catch (...) {}
    }
    return common::Result<void>::success();
}

std::shared_ptr<rtc::PeerConnection> PeerConnectionManager::getPeerConnection() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return pc_;
}

bool PeerConnectionManager::getSelectedCandidatePair(std::string* localType, std::string* remoteType) const {
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pc = pc_;
    }
    if (!pc) return false;

    rtc::Candidate localCand;
    rtc::Candidate remoteCand;
    if (pc->getSelectedCandidatePair(&localCand, &remoteCand)) {
        if (localType) {
            switch (localCand.type()) {
                case rtc::Candidate::Type::Host: *localType = "host"; break;
                case rtc::Candidate::Type::ServerReflexive: *localType = "srflx"; break;
                case rtc::Candidate::Type::PeerReflexive: *localType = "prflx"; break;
                case rtc::Candidate::Type::Relayed: *localType = "relay"; break;
                default: *localType = "unknown"; break;
            }
        }
        if (remoteType) {
            switch (remoteCand.type()) {
                case rtc::Candidate::Type::Host: *remoteType = "host"; break;
                case rtc::Candidate::Type::ServerReflexive: *remoteType = "srflx"; break;
                case rtc::Candidate::Type::PeerReflexive: *remoteType = "prflx"; break;
                case rtc::Candidate::Type::Relayed: *remoteType = "relay"; break;
                default: *remoteType = "unknown"; break;
            }
        }
        return true;
    }
    return false;
}

common::Result<void> PeerConnectionManager::restartIce() {
    std::shared_ptr<rtc::PeerConnection> pc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!iceRestartController_.canAttemptRestart()) {
            return common::Error(common::SubsystemId::Transport, common::ErrorCode::ResourceExhausted,
                                 "ICE restart rate-limited or maximum retries exceeded (" +
                                 std::to_string(iceRestartController_.getAttemptCount()) + ")");
        }
        iceRestartController_.recordRestartAttempt();
        pc = pc_;
    }
    if (!pc) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidState, "PeerConnection is null");
    }
    try {
        pc->setLocalDescription(rtc::Description::Type::Offer, {.iceUfrag = std::nullopt, .icePwd = std::nullopt});
        MIST_LOG_INFO(common::SubsystemId::Transport, "ICE restart initiated.");
        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Failed to restart ICE: " + std::string(e.what()));
    }
}

std::vector<ParsedIceCandidate> PeerConnectionManager::getLocalCandidates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return localParsedCandidates_;
}

std::vector<ParsedIceCandidate> PeerConnectionManager::getRemoteCandidates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remoteParsedCandidates_;
}

NatDiagnostics PeerConnectionManager::getNatDiagnostics() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    NatDiagnostics diag{};
    diag.detectedNatType = IceCandidatePrioritizer::classifyNatBehavior(localParsedCandidates_);

    for (const auto& c : localParsedCandidates_) {
        if (c.type == CandidateType::Host) diag.hostCandidatesCount++;
        else if (c.type == CandidateType::ServerReflexive) diag.srflxCandidatesCount++;
        else if (c.type == CandidateType::Relayed) diag.relayCandidatesCount++;

        if (c.family == AddressFamily::IPv4) diag.ipv4CandidatesCount++;
        else if (c.family == AddressFamily::IPv6) diag.ipv6CandidatesCount++;

        if (c.isCgnat) diag.isBehindCgnat = true;
    }

    std::string locType, remType;
    if (getSelectedCandidatePair(&locType, &remType)) {
        diag.activeLocalCandidateType = locType;
        diag.activeRemoteCandidateType = remType;
        diag.selectedPairSummary = locType + " <-> " + remType;
        diag.hasDirectRoute = (locType != "relay" && remType != "relay");
    }

    bool bothSymmetric = (diag.detectedNatType == NatType::Symmetric);
    diag.failureReason = IceCandidatePrioritizer::classifyFailure(
        true,
        !localParsedCandidates_.empty(),
        (state_ == WebRtcState::Connected),
        bothSymmetric
    );

    return diag;
}

} // namespace mist::transport
