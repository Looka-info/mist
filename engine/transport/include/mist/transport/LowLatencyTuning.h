#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mist::transport {

struct LowLatencyProfile {
    bool enablePacedSender{true};
    double pacingFactor{1.15};           // 115% pacing headroom over target bitrate
    uint32_t maxQueueDurationMs{33};     // Max 2 frames @ 60fps before dropping stale video
    uint32_t maxQueuePackets{256};       // Queue depth bound

    uint32_t minPlayoutDelayFrames{0};   // 0 frames (instant display)
    uint32_t maxPlayoutDelayFrames{1};   // Max 1 frame jitter buffer delay (16.6ms @ 60fps)

    bool enableFastNack{true};
    uint32_t nackRttThresholdMs{60};     // Beyond 60ms RTT, NACKs arrive too late; escalate to PLI
    uint32_t pliDebounceIntervalMs{200}; // Minimum ms between consecutive PLI requests

    uint32_t rtcpReportIntervalMs{100};  // High-frequency RTCP feedback (vs standard 1000ms)
    bool enableReducedSizeRtcp{true};
};

class LowLatencyTuner {
public:
    // Injects the WebRTC playout delay RTP header extension into SDP
    // URI: http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
    [[nodiscard]] static std::string injectPlayoutDelay(
        const std::string& sdp,
        uint32_t minFrames = 0,
        uint32_t maxFrames = 1,
        uint32_t targetFps = 60
    ) noexcept;

    // Parses playout delay bounds (in milliseconds) from SDP
    [[nodiscard]] static std::pair<uint32_t, uint32_t> parsePlayoutDelayMs(
        const std::string& sdp
    ) noexcept;

    // Computes optimal RTCP report interval based on RTT, FPS and active guest count
    [[nodiscard]] static uint32_t calculateOptimalRtcpIntervalMs(
        uint32_t rttMs,
        uint32_t targetFps = 60,
        uint32_t guestCount = 1
    ) noexcept;
};

} // namespace mist::transport
