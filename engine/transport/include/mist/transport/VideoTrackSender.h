#pragma once

#include "mist/common/Result.h"
#include "mist/encode/IVideoEncoder.h"
#include "mist/transport/WebRtcConfig.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>

namespace rtc {
class Track;
class PeerConnection;
class RtpPacketizationConfig;
}

namespace mist::transport {

using KeyframeRequestCallback = std::function<void()>;

class VideoTrackSender {
public:
    VideoTrackSender() noexcept;
    ~VideoTrackSender();

    [[nodiscard]] common::Result<void> initialize(
        std::shared_ptr<rtc::PeerConnection> pc,
        const WebRtcTransportConfig& config
    );

    // Enqueue an encoded H.264 access unit from the host encoder.
    // Drops oldest if queue depth >= 2 to strictly bound latency (ADR 0014).
    [[nodiscard]] common::Result<void> sendFrame(const encode::EncodedPacket& packet);

    void setKeyframeRequestCallback(KeyframeRequestCallback cb) { keyframeCb_ = std::move(cb); }
    [[nodiscard]] bool isTrackOpen() const noexcept;
    [[nodiscard]] uint64_t getFramesSent() const noexcept { return framesSent_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getFramesDropped() const noexcept { return framesDropped_.load(std::memory_order_relaxed); }

    void close() noexcept;

private:
    mutable std::mutex mutex_;
    WebRtcTransportConfig config_;
    std::shared_ptr<rtc::Track> track_;
    std::shared_ptr<rtc::RtpPacketizationConfig> rtpConfig_;
    KeyframeRequestCallback keyframeCb_;
    uint64_t frameIndex_{0};

    std::atomic<uint64_t> framesSent_{0};
    std::atomic<uint64_t> framesDropped_{0};
    std::atomic<bool> isOpen_{false};
};

} // namespace mist::transport
