#include "mist/transport/VideoTrackSender.h"
#include "mist/common/Logger.h"
#include "rtc/rtc.hpp"
#include "rtc/rembhandler.hpp"

namespace mist::transport {

VideoTrackSender::VideoTrackSender() noexcept = default;

VideoTrackSender::~VideoTrackSender() {
    close();
}

common::Result<void> VideoTrackSender::initialize(
    std::shared_ptr<rtc::PeerConnection> pc,
    const WebRtcTransportConfig& config
) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    if (!pc) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument, "PeerConnection is null");
    }

    try {
        bool isHevc = (config_.videoCodec == "hevc" || config_.videoCodec == "h265");
        int payloadType = isHevc ? 98 : 96;

        rtc::Description::Video mediaDesc("video", rtc::Description::Direction::SendOnly);
        if (isHevc) {
            mediaDesc.addH265Codec(payloadType);
        } else {
            mediaDesc.addH264Codec(payloadType);
        }
        mediaDesc.addSSRC(1, "mist-video");

        track_ = pc->addTrack(mediaDesc);

        rtpConfig_ = std::make_shared<rtc::RtpPacketizationConfig>(
            /*ssrc=*/1,
            /*cname=*/"mist-video",
            /*payloadType=*/payloadType,
            /*clockRate=*/config_.videoClockRate
        );

        if (isHevc) {
            auto packetizer = std::make_shared<rtc::H265RtpPacketizer>(
                rtc::NalUnit::Separator::StartSequence,
                rtpConfig_,
                /*maxFragmentSize=*/1200
            );
            track_->setMediaHandler(packetizer);
        } else {
            auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(
                rtc::NalUnit::Separator::StartSequence,
                rtpConfig_,
                /*maxFragmentSize=*/1200
            );
            track_->setMediaHandler(packetizer);
        }

        auto pliHandler = std::make_shared<rtc::PliHandler>([this]() {
            MIST_LOG_INFO(common::SubsystemId::Transport, "Received RTCP PLI (Picture Loss Indication) keyframe request");
            if (keyframeCb_) {
                keyframeCb_();
            }
        });
        track_->chainMediaHandler(pliHandler);

        auto rembHandler = std::make_shared<rtc::RembHandler>([this](unsigned int bitrate) {
            MIST_LOG_INFO(common::SubsystemId::Transport, "Received RTCP REMB request: " + std::to_string(bitrate) + " bps");
            if (rembCb_) {
                rembCb_(static_cast<uint32_t>(bitrate));
            }
        });
        track_->chainMediaHandler(rembHandler);

        track_->onOpen([this]() {
            isOpen_.store(true);
            MIST_LOG_INFO(common::SubsystemId::Transport, "VideoTrackSender opened and ready.");
        });

        track_->onClosed([this]() {
            isOpen_.store(false);
            MIST_LOG_INFO(common::SubsystemId::Transport, "VideoTrackSender closed.");
        });

        track_->onError([](const std::string& error) {
            MIST_LOG_ERROR(common::SubsystemId::Transport, "VideoTrackSender error: " + error);
        });

        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Failed to initialize VideoTrackSender: " + std::string(e.what()));
    }
}

common::Result<void> VideoTrackSender::sendFrame(const encode::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!track_ || !isOpen_.load()) {
        framesDropped_.fetch_add(1, std::memory_order_relaxed);
        return common::Result<void>::success();
    }

    if (packet.data.empty()) {
        return common::Result<void>::success();
    }

    // Advance RTP timestamp at videoClockRate for every frame
    if (rtpConfig_) {
        frameIndex_++;
        uint32_t rtpTimestamp = 0;
        if (packet.timestampNs > 0) {
            rtpTimestamp = static_cast<uint32_t>((packet.timestampNs * static_cast<uint64_t>(config_.videoClockRate)) / 1000000000ULL);
        } else {
            rtpTimestamp = static_cast<uint32_t>(frameIndex_ * (config_.videoClockRate / (config_.targetFps > 0 ? config_.targetFps : 60)));
        }
        if (rtpTimestamp == 0) {
            rtpTimestamp = static_cast<uint32_t>(frameIndex_ * (config_.videoClockRate / 60));
        }
        rtpConfig_->timestamp = rtpTimestamp;
    }

    try {
        bool ok = track_->send(reinterpret_cast<const rtc::byte*>(packet.data.data()), packet.data.size());
        if (ok) {
            framesSent_.fetch_add(1, std::memory_order_relaxed);
        } else {
            framesDropped_.fetch_add(1, std::memory_order_relaxed);
        }
    } catch (const std::exception& e) {
        framesDropped_.fetch_add(1, std::memory_order_relaxed);
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Track send exception: " + std::string(e.what()));
    }

    return common::Result<void>::success();
}

bool VideoTrackSender::isTrackOpen() const noexcept {
    return isOpen_.load();
}

void VideoTrackSender::close() noexcept {
    std::shared_ptr<rtc::Track> tr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tr = std::move(track_);
        rtpConfig_.reset();
    }
    if (tr) {
        try {
            tr->close();
        } catch (...) {}
    }
    isOpen_.store(false);
}

} // namespace mist::transport
