#pragma once

#include "mist/transport/ITransport.h"
#include "mist/transport/WebRtcConfig.h"
#include "mist/transport/TransportStatistics.h"
#include "mist/transport/PeerConnectionManager.h"
#include "mist/transport/VideoTrackSender.h"
#include "mist/transport/VideoTrackReceiver.h"
#include "mist/transport/DataChannelManager.h"
#include "mist/services/ISignalingClient.h"
#include <memory>
#include <mutex>

namespace mist::transport {

class WebRtcTransport : public ITransport {
public:
    WebRtcTransport() noexcept;
    ~WebRtcTransport() override;

    [[nodiscard]] static std::unique_ptr<WebRtcTransport> create() noexcept;

    // Standard ITransport implementation
    [[nodiscard]] common::Result<void> initialize(const TransportConfig& config) override;
    [[nodiscard]] common::Result<void> connect() override;
    [[nodiscard]] common::Result<void> disconnect() override;
    [[nodiscard]] common::Result<void> send(TransportChannel channel, std::span<const uint8_t> data) override;
    void setDataReceivedCallback(DataReceivedCallback callback) override;
    [[nodiscard]] bool isConnected() const noexcept override;

    // WebRTC-specific initialization and control
    [[nodiscard]] common::Result<void> initializeWebRtc(
        const WebRtcTransportConfig& config,
        std::shared_ptr<services::ISignalingClient> signalingClient = nullptr
    );

    [[nodiscard]] common::Result<void> createOffer();
    [[nodiscard]] common::Result<void> sendVideoPacket(const encode::EncodedPacket& packet);
    void setKeyframeRequestCallback(KeyframeRequestCallback cb);
    void setFrameReceivedCallback(FrameReceivedCallback cb);

    [[nodiscard]] TransportStatsSnapshot getStatsSnapshot() const noexcept;
    [[nodiscard]] const WebRtcTransportConfig& getConfig() const noexcept { return config_; }

    [[nodiscard]] std::shared_ptr<PeerConnectionManager> getPeerConnectionManager() const noexcept { return pcManager_; }
    [[nodiscard]] std::shared_ptr<VideoTrackSender> getVideoSender() const noexcept { return videoSender_; }
    [[nodiscard]] std::shared_ptr<VideoTrackReceiver> getVideoReceiver() const noexcept { return videoReceiver_; }
    [[nodiscard]] std::shared_ptr<DataChannelManager> getDataChannelManager() const noexcept { return dcManager_; }

private:
    void setupSignalingRouting();

    mutable std::mutex mutex_;
    WebRtcTransportConfig config_;
    TransportConfig genericConfig_;

    std::shared_ptr<PeerConnectionManager> pcManager_;
    std::shared_ptr<VideoTrackSender> videoSender_;
    std::shared_ptr<VideoTrackReceiver> videoReceiver_;
    std::shared_ptr<DataChannelManager> dcManager_;
    std::shared_ptr<services::ISignalingClient> signalingClient_;

    std::shared_ptr<TransportStatistics> stats_;
    DataReceivedCallback genericDataCb_;
    bool isInitialized_{false};
};

} // namespace mist::transport
