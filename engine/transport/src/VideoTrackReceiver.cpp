#include "mist/transport/VideoTrackReceiver.h"
#include "mist/common/Logger.h"
#include "rtc/rtc.hpp"
#include <cstring>

namespace mist::transport {

VideoTrackReceiver::VideoTrackReceiver() noexcept = default;

VideoTrackReceiver::~VideoTrackReceiver() {
    close();
}

common::Result<void> VideoTrackReceiver::initialize(
    std::shared_ptr<rtc::PeerConnection> pc,
    const WebRtcTransportConfig& config
) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    if (!pc) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument, "PeerConnection is null");
    }

    try {
        pc->onTrack([this](std::shared_ptr<rtc::Track> track) {
            {
                std::lock_guard<std::mutex> innerLock(mutex_);
                track_ = track;
            }
            MIST_LOG_INFO(common::SubsystemId::Transport, "Incoming remote VideoTrack received (mid: " + track->mid() + ")");

            auto depacketizer = std::make_shared<rtc::H264RtpDepacketizer>(rtc::NalUnit::Separator::StartSequence);
            track->setMediaHandler(depacketizer);

            track->onOpen([this]() {
                isOpen_.store(true);
                MIST_LOG_INFO(common::SubsystemId::Transport, "VideoTrackReceiver opened and ready.");
            });

            track->onClosed([this]() {
                isOpen_.store(false);
                MIST_LOG_INFO(common::SubsystemId::Transport, "VideoTrackReceiver closed.");
            });

            track->onError([](const std::string& error) {
                MIST_LOG_ERROR(common::SubsystemId::Transport, "VideoTrackReceiver error: " + error);
            });

            track->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
                if (data.empty()) return;
                framesReceived_.fetch_add(1, std::memory_order_relaxed);

                // Reconstruct in-memory frame buffer (zero disk I/O)
                std::vector<uint8_t> frameBytes(data.size());
                std::memcpy(frameBytes.data(), data.data(), data.size());
                uint64_t timestampNs = info.timestamp;
                FrameReceivedCallback cb;
                {
                    std::lock_guard<std::mutex> innerLock(mutex_);
                    cb = frameCb_;
                }
                if (cb) {
                    cb(frameBytes, timestampNs);
                }
            });
        });

        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Failed to initialize VideoTrackReceiver: " + std::string(e.what()));
    }
}

void VideoTrackReceiver::requestKeyframe() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (track_ && isOpen_.load()) {
        try {
            track_->requestKeyframe();
            MIST_LOG_DEBUG(common::SubsystemId::Transport, "Sent RTCP PLI request for IDR keyframe");
        } catch (...) {}
    }
}

bool VideoTrackReceiver::isTrackOpen() const noexcept {
    return isOpen_.load();
}

void VideoTrackReceiver::close() noexcept {
    std::shared_ptr<rtc::Track> tr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tr = std::move(track_);
    }
    if (tr) {
        try {
            tr->close();
        } catch (...) {}
    }
    isOpen_.store(false);
}

} // namespace mist::transport
