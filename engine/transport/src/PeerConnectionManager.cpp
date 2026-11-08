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
        rtcConfig.iceServers.emplace_back(s.url);
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
        return common::Result<void>::success();
    }

    try {
        MIST_LOG_INFO(common::SubsystemId::Transport, "Adding remote candidate (mid: " + mid + "): " + candidate);
        rtc::Candidate cand(candidate);
        if (!mid.empty()) {
            cand.hintMid(mid);
        }
        pc_->addRemoteCandidate(cand);
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

} // namespace mist::transport
