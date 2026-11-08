#include "mist/transport/AudioTrackSender.h"
#include "mist/common/Logger.h"
#include "rtc/rtc.hpp"

namespace mist::transport {

AudioTrackSender::AudioTrackSender() noexcept = default;

AudioTrackSender::~AudioTrackSender() {
    close();
}

common::Result<void> AudioTrackSender::initialize(
    std::shared_ptr<rtc::PeerConnection> pc,
    const WebRtcTransportConfig& config
) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    if (!pc) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument, "PeerConnection is null");
    }

    try {
        rtc::Description::Audio mediaDesc("audio", rtc::Description::Direction::SendOnly);
        mediaDesc.addOpusCodec(111);
        mediaDesc.addSSRC(2, "mist-stream");

        track_ = pc->addTrack(mediaDesc);

        rtpConfig_ = std::make_shared<rtc::RtpPacketizationConfig>(
            /*ssrc=*/2,
            /*cname=*/"mist-stream",
            /*payloadType=*/111,
            /*clockRate=*/48000
        );

        auto packetizer = std::make_shared<rtc::OpusRtpPacketizer>(rtpConfig_);
        track_->setMediaHandler(packetizer);

        track_->onOpen([this]() {
            isOpen_.store(true);
            MIST_LOG_INFO(common::SubsystemId::Transport, "AudioTrackSender opened and ready.");
        });

        track_->onClosed([this]() {
            isOpen_.store(false);
            MIST_LOG_INFO(common::SubsystemId::Transport, "AudioTrackSender closed.");
        });

        track_->onError([](const std::string& error) {
            MIST_LOG_ERROR(common::SubsystemId::Transport, "AudioTrackSender error: " + error);
        });

        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Failed to initialize AudioTrackSender: " + std::string(e.what()));
    }
}

common::Result<void> AudioTrackSender::sendFrame(std::span<const uint8_t> opusData, uint32_t sampleCount) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!track_ || !isOpen_.load()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidState, "Audio track is not open");
    }
    if (opusData.empty()) {
        return common::Result<void>::success();
    }

    try {
        if (rtpConfig_) {
            rtpConfig_->timestamp = static_cast<uint32_t>(sampleIndex_);
            sampleIndex_ += sampleCount;
        }

        const auto* byteData = reinterpret_cast<const std::byte*>(opusData.data());
        track_->send(byteData, opusData.size());

        packetsSent_.fetch_add(1, std::memory_order_relaxed);
        bytesSent_.fetch_add(opusData.size(), std::memory_order_relaxed);

        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NetworkError,
                             "AudioTrackSender::sendFrame failed: " + std::string(e.what()));
    }
}

bool AudioTrackSender::isTrackOpen() const noexcept {
    return isOpen_.load(std::memory_order_relaxed);
}

void AudioTrackSender::close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    isOpen_.store(false);
    if (track_) {
        try {
            track_->close();
        } catch (...) {
            // Suppress exception on shutdown
        }
        track_.reset();
    }
    rtpConfig_.reset();
}

} // namespace mist::transport
