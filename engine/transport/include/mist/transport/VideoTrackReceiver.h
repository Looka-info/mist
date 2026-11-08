#pragma once

#include "mist/common/Result.h"
#include "mist/decode/IVideoDecoder.h"
#include "mist/transport/WebRtcConfig.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>

namespace rtc {
class Track;
class PeerConnection;
}

namespace mist::transport {

// Callback delivering reassembled in-memory H.264 access units directly to decoder (zero disk writes)
using FrameReceivedCallback = std::function<void(const std::vector<uint8_t>& accessUnit, uint64_t timestampNs)>;

class VideoTrackReceiver {
public:
    VideoTrackReceiver() noexcept;
    ~VideoTrackReceiver();

    [[nodiscard]] common::Result<void> initialize(
        std::shared_ptr<rtc::PeerConnection> pc,
        const WebRtcTransportConfig& config
    );

    void attachTrack(std::shared_ptr<rtc::Track> track);

    void setFrameReceivedCallback(FrameReceivedCallback cb) { frameCb_ = std::move(cb); }
    void requestKeyframe() noexcept;

    [[nodiscard]] bool isTrackOpen() const noexcept;
    [[nodiscard]] uint64_t getFramesReceived() const noexcept { return framesReceived_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getFramesDropped() const noexcept { return framesDropped_.load(std::memory_order_relaxed); }

    void close() noexcept;

private:
    void handleIncomingFrame(const std::vector<uint8_t>& frameData, uint64_t timestampNs);

    mutable std::mutex mutex_;
    WebRtcTransportConfig config_;
    std::shared_ptr<rtc::Track> track_;
    FrameReceivedCallback frameCb_;

    std::atomic<uint64_t> framesReceived_{0};
    std::atomic<uint64_t> framesDropped_{0};
    std::atomic<bool> isOpen_{false};
};

} // namespace mist::transport
