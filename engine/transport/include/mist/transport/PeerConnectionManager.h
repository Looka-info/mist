#pragma once

#include "mist/common/Result.h"
#include "mist/transport/WebRtcConfig.h"
#include "mist/transport/NatTraversal.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <mutex>

namespace rtc {
class PeerConnection;
}

namespace mist::transport {

using LocalDescriptionCallback = std::function<void(const std::string& type, const std::string& sdp)>;
using LocalCandidateCallback = std::function<void(const std::string& candidate, const std::string& mid)>;
using StateChangeCallback = std::function<void(WebRtcState state)>;

class PeerConnectionManager {
public:
    PeerConnectionManager() noexcept;
    ~PeerConnectionManager();

    [[nodiscard]] common::Result<void> initialize(const WebRtcTransportConfig& config);
    [[nodiscard]] common::Result<void> createOffer();
    [[nodiscard]] common::Result<void> setRemoteDescription(const std::string& type, const std::string& sdp);
    [[nodiscard]] common::Result<void> addRemoteCandidate(const std::string& candidate, const std::string& mid);
    [[nodiscard]] common::Result<void> close();

    void setLocalDescriptionCallback(LocalDescriptionCallback cb);
    void setLocalCandidateCallback(LocalCandidateCallback cb);
    void setStateChangeCallback(StateChangeCallback cb);

    [[nodiscard]] std::shared_ptr<rtc::PeerConnection> getPeerConnection() const noexcept;
    [[nodiscard]] WebRtcState getState() const noexcept { return state_; }
    [[nodiscard]] bool isConnected() const noexcept { return state_ == WebRtcState::Connected; }
    [[nodiscard]] bool getSelectedCandidatePair(std::string* localType, std::string* remoteType) const;
    [[nodiscard]] common::Result<void> restartIce();
    [[nodiscard]] NatDiagnostics getNatDiagnostics() const noexcept;
    [[nodiscard]] std::vector<ParsedIceCandidate> getLocalCandidates() const;
    [[nodiscard]] std::vector<ParsedIceCandidate> getRemoteCandidates() const;
    [[nodiscard]] IceRestartController& getIceRestartController() noexcept { return iceRestartController_; }
    [[nodiscard]] PortMappingManager& getPortMappingManager() noexcept { return portMappingManager_; }

private:
    mutable std::mutex mutex_;
    WebRtcTransportConfig config_;
    std::shared_ptr<rtc::PeerConnection> pc_;
    WebRtcState state_{WebRtcState::New};
    LocalDescriptionCallback localDescCb_;
    LocalCandidateCallback localCandidateCb_;
    StateChangeCallback stateChangeCb_;
    std::optional<std::pair<std::string, std::string>> pendingLocalDesc_;
    std::vector<std::pair<std::string, std::string>> pendingCandidates_;
    bool hasRemoteDescription_{false};
    std::vector<std::pair<std::string, std::string>> pendingRemoteCandidates_;
    std::vector<ParsedIceCandidate> localParsedCandidates_;
    std::vector<ParsedIceCandidate> remoteParsedCandidates_;
    IceRestartController iceRestartController_;
    PortMappingManager portMappingManager_;
};

} // namespace mist::transport
