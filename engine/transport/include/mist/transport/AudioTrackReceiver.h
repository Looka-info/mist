#pragma once

#include "mist/common/Result.h"
#include "mist/transport/WebRtcConfig.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <span>
#include <vector>

namespace rtc {
class Track;
class RtpDepacketizer;
struct FrameInfo;
}

namespace mist::transport {

struct AudioFrameInfo {
    uint32_t rtpTimestamp{0};
    uint16_t sequenceNumber{0};
    uint64_t arrivalTimeNs{0};
};

using AudioFrameCallback = std::function<void(std::span<const uint8_t> opusPayload, const AudioFrameInfo& info)>;

class AudioTrackReceiver {
public:
    AudioTrackReceiver() noexcept;
    ~AudioTrackReceiver();

    [[nodiscard]] common::Result<void> initialize(const WebRtcTransportConfig& config);
    void attachTrack(std::shared_ptr<rtc::Track> track);

    void setFrameCallback(AudioFrameCallback cb);

    [[nodiscard]] bool isTrackOpen() const noexcept;
    [[nodiscard]] uint64_t getPacketsReceived() const noexcept { return packetsReceived_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getBytesReceived() const noexcept { return bytesReceived_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t getLastRtpTimestamp() const noexcept { return lastRtpTimestamp_.load(std::memory_order_relaxed); }

    void close() noexcept;

private:
    mutable std::mutex mutex_;
    WebRtcTransportConfig config_;
    std::shared_ptr<rtc::Track> track_;
    AudioFrameCallback frameCb_;

    std::atomic<uint64_t> packetsReceived_{0};
    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<uint32_t> lastRtpTimestamp_{0};
    std::atomic<bool> isOpen_{false};
};

} // namespace mist::transport
