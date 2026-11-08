#pragma once

#include "mist/common/Result.h"
#include "mist/transport/WebRtcConfig.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <span>

namespace rtc {
class Track;
class PeerConnection;
class RtpPacketizationConfig;
}

namespace mist::transport {

class AudioTrackSender {
public:
    AudioTrackSender() noexcept;
    ~AudioTrackSender();

    [[nodiscard]] common::Result<void> initialize(
        std::shared_ptr<rtc::PeerConnection> pc,
        const WebRtcTransportConfig& config
    );

    // Send an encoded Opus audio packet
    [[nodiscard]] common::Result<void> sendFrame(std::span<const uint8_t> opusData, uint32_t sampleCount = 480);

    [[nodiscard]] bool isTrackOpen() const noexcept;
    [[nodiscard]] uint64_t getPacketsSent() const noexcept { return packetsSent_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getBytesSent() const noexcept { return bytesSent_.load(std::memory_order_relaxed); }

    void close() noexcept;

private:
    mutable std::mutex mutex_;
    WebRtcTransportConfig config_;
    std::shared_ptr<rtc::Track> track_;
    std::shared_ptr<rtc::RtpPacketizationConfig> rtpConfig_;
    uint64_t sampleIndex_{0};

    std::atomic<uint64_t> packetsSent_{0};
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<bool> isOpen_{false};
};

} // namespace mist::transport
