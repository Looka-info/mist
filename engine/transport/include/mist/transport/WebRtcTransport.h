#pragma once

#include "mist/transport/ITransport.h"
#include "mist/transport/WebRtcConfig.h"
#include "mist/transport/TransportStatistics.h"
#include "mist/transport/PeerConnectionManager.h"
#include "mist/transport/VideoTrackSender.h"
#include "mist/transport/VideoTrackReceiver.h"
#include "mist/transport/AudioTrackSender.h"
#include "mist/transport/AudioTrackReceiver.h"
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
    [[nodiscard]] common::Result<void> sendAudioFrame(std::span<const uint8_t> opusPayload, uint32_t sampleCount = 480);
    void setKeyframeRequestCallback(KeyframeRequestCallback cb);
    void setFrameReceivedCallback(FrameReceivedCallback cb);
    void setAudioFrameCallback(AudioFrameCallback cb);

    using InputDataCallback = std::function<void(const std::string& label, std::span<const uint8_t> data)>;
    [[nodiscard]] common::Result<void> sendInputData(const std::string& label, std::span<const uint8_t> data);
    void setInputDataCallback(InputDataCallback cb);

    [[nodiscard]] TransportStatsSnapshot getStatsSnapshot() const noexcept;
    [[nodiscard]] const WebRtcTransportConfig& getConfig() const noexcept { return config_; }

    [[nodiscard]] std::shared_ptr<PeerConnectionManager> getPeerConnectionManager() const noexcept { return pcManager_; }
    [[nodiscard]] std::shared_ptr<VideoTrackSender> getVideoSender() const noexcept { return videoSender_; }
    [[nodiscard]] std::shared_ptr<VideoTrackReceiver> getVideoReceiver() const noexcept { return videoReceiver_; }
    [[nodiscard]] std::shared_ptr<AudioTrackSender> getAudioSender() const noexcept { return audioSender_; }
    [[nodiscard]] std::shared_ptr<AudioTrackReceiver> getAudioReceiver() const noexcept { return audioReceiver_; }
    [[nodiscard]] std::shared_ptr<DataChannelManager> getDataChannelManager() const noexcept { return dcManager_; }

private:
    void setupSignalingRouting();

    mutable std::mutex mutex_;
    WebRtcTransportConfig config_;
    TransportConfig genericConfig_;

    std::shared_ptr<PeerConnectionManager> pcManager_;
    std::shared_ptr<VideoTrackSender> videoSender_;
    std::shared_ptr<VideoTrackReceiver> videoReceiver_;
    std::shared_ptr<AudioTrackSender> audioSender_;
    std::shared_ptr<AudioTrackReceiver> audioReceiver_;
    std::shared_ptr<DataChannelManager> dcManager_;
    std::shared_ptr<services::ISignalingClient> signalingClient_;

    std::shared_ptr<TransportStatistics> stats_;
    DataReceivedCallback genericDataCb_;
    AudioFrameCallback audioFrameCb_;
    InputDataCallback inputDataCb_;
    bool isInitialized_{false};
};

} // namespace mist::transport
