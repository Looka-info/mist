#pragma once

#include "mist/common/Result.h"
#include "mist/encode/IVideoEncoder.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <span>
#include <chrono>

namespace mist::transport {

class WebRtcTransport;

struct GuestEndpointStats {
    std::string guestId;
    uint64_t framesSent{0};
    uint64_t framesDropped{0};
    uint64_t bytesSent{0};
    uint64_t audioFramesSent{0};
    uint64_t audioFramesDropped{0};
    uint32_t simulatedQueueDepth{0};
    uint32_t maxQueueDepth{2};
    bool isSlowClient{false};
    uint32_t consecutiveDrops{0};
    uint64_t lastFrameTimestampNs{0};
    bool isConnected{true};
};

struct MultiGuestFanoutSnapshot {
    size_t totalGuests{0};
    size_t activeGuests{0};
    size_t slowGuests{0};
    uint64_t totalFramesBroadcast{0};
    uint64_t totalFramesDelivered{0};
    uint64_t totalFramesDropped{0};
    uint64_t totalAudioBroadcast{0};
    std::vector<GuestEndpointStats> guestStats;
};

using FrameSenderFunc = std::function<common::Result<void>(const encode::EncodedPacket&)>;
using AudioSenderFunc = std::function<common::Result<void>(std::span<const uint8_t>, uint32_t)>;
using GlobalKeyframeCallback = std::function<void()>;

class MultiGuestFanout {
public:
    explicit MultiGuestFanout(uint32_t defaultMaxQueueDepth = 2, uint32_t keyframeDebounceMs = 250) noexcept;
    ~MultiGuestFanout();

    // Guest registration
    [[nodiscard]] common::Result<void> registerGuest(
        const std::string& guestId,
        std::shared_ptr<WebRtcTransport> transport,
        uint32_t maxQueueDepth = 2
    );

    [[nodiscard]] common::Result<void> registerGuestSender(
        const std::string& guestId,
        FrameSenderFunc videoSender,
        AudioSenderFunc audioSender = nullptr,
        uint32_t maxQueueDepth = 2
    );

    [[nodiscard]] common::Result<void> unregisterGuest(const std::string& guestId);
    [[nodiscard]] bool hasGuest(const std::string& guestId) const noexcept;
    [[nodiscard]] size_t getGuestCount() const noexcept;
    [[nodiscard]] std::vector<std::string> getGuestIds() const;

    // Broadcasting: Encode-once, send-to-many with slow-client isolation
    [[nodiscard]] common::Result<void> broadcastVideoFrame(const encode::EncodedPacket& packet);
    [[nodiscard]] common::Result<void> broadcastAudioFrame(std::span<const uint8_t> opusPayload, uint32_t sampleCount = 480);

    // Keyframe coordination across guests
    void setKeyframeRequestCallback(GlobalKeyframeCallback cb);
    void requestKeyframe(const std::string& guestId);

    // Guest slow-client testing & controls
    void setGuestSimulatedQueueDepth(const std::string& guestId, uint32_t depth);
    void setGuestConnected(const std::string& guestId, bool connected);

    // Diagnostics & stats
    [[nodiscard]] common::Result<GuestEndpointStats> getGuestStats(const std::string& guestId) const;
    [[nodiscard]] MultiGuestFanoutSnapshot getSnapshot() const;
    void resetStats();

private:
    struct GuestEntry {
        std::string guestId;
        std::shared_ptr<WebRtcTransport> transport;
        FrameSenderFunc videoSender;
        AudioSenderFunc audioSender;
        GuestEndpointStats stats;
        bool isCustomSender{false};
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, GuestEntry> guests_;
    uint32_t defaultMaxQueueDepth_{2};
    uint32_t keyframeDebounceMs_{250};
    std::chrono::steady_clock::time_point lastKeyframeTime_{};
    bool hasSentKeyframe_{false};
    GlobalKeyframeCallback keyframeCb_;

    uint64_t totalFramesBroadcast_{0};
    uint64_t totalAudioBroadcast_{0};
};

} // namespace mist::transport
