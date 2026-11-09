#include "mist/transport/MultiGuestFanout.h"
#include "mist/transport/WebRtcTransport.h"
#include "mist/common/Logger.h"

namespace mist::transport {

MultiGuestFanout::MultiGuestFanout(uint32_t defaultMaxQueueDepth, uint32_t keyframeDebounceMs) noexcept
    : defaultMaxQueueDepth_(defaultMaxQueueDepth), keyframeDebounceMs_(keyframeDebounceMs) {}

MultiGuestFanout::~MultiGuestFanout() = default;

common::Result<void> MultiGuestFanout::registerGuest(
    const std::string& guestId,
    std::shared_ptr<WebRtcTransport> transport,
    uint32_t maxQueueDepth
) {
    if (guestId.empty()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument,
                             "Guest ID cannot be empty");
    }
    if (!transport) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument,
                             "WebRtcTransport cannot be null");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (guests_.find(guestId) != guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::AlreadyExists,
                             "Guest ID already registered: " + guestId);
    }

    GuestEntry entry{};
    entry.guestId = guestId;
    entry.transport = transport;
    entry.isCustomSender = false;
    entry.stats.guestId = guestId;
    entry.stats.maxQueueDepth = (maxQueueDepth > 0) ? maxQueueDepth : defaultMaxQueueDepth_;
    entry.stats.isConnected = true;

    // Route guest RTCP PLI/FIR keyframe requests to fanout coordinator
    transport->setKeyframeRequestCallback([this, id = guestId]() {
        requestKeyframe(id);
    });

    guests_.emplace(guestId, std::move(entry));
    MIST_LOG_INFO(common::SubsystemId::Transport, "MultiGuestFanout: Registered guest endpoint " + guestId);
    return common::Result<void>::success();
}

common::Result<void> MultiGuestFanout::registerGuestSender(
    const std::string& guestId,
    FrameSenderFunc videoSender,
    AudioSenderFunc audioSender,
    uint32_t maxQueueDepth
) {
    if (guestId.empty()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument,
                             "Guest ID cannot be empty");
    }
    if (!videoSender) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument,
                             "FrameSenderFunc cannot be null");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (guests_.find(guestId) != guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::AlreadyExists,
                             "Guest ID already registered: " + guestId);
    }

    GuestEntry entry{};
    entry.guestId = guestId;
    entry.videoSender = std::move(videoSender);
    entry.audioSender = std::move(audioSender);
    entry.isCustomSender = true;
    entry.stats.guestId = guestId;
    entry.stats.maxQueueDepth = (maxQueueDepth > 0) ? maxQueueDepth : defaultMaxQueueDepth_;
    entry.stats.isConnected = true;

    guests_.emplace(guestId, std::move(entry));
    MIST_LOG_INFO(common::SubsystemId::Transport, "MultiGuestFanout: Registered custom sender guest " + guestId);
    return common::Result<void>::success();
}

common::Result<void> MultiGuestFanout::unregisterGuest(const std::string& guestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it == guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound,
                             "Guest ID not found: " + guestId);
    }
    guests_.erase(it);
    MIST_LOG_INFO(common::SubsystemId::Transport, "MultiGuestFanout: Unregistered guest " + guestId);
    return common::Result<void>::success();
}

bool MultiGuestFanout::hasGuest(const std::string& guestId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return guests_.find(guestId) != guests_.end();
}

size_t MultiGuestFanout::getGuestCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return guests_.size();
}

std::vector<std::string> MultiGuestFanout::getGuestIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(guests_.size());
    for (const auto& [id, _] : guests_) {
        ids.push_back(id);
    }
    return ids;
}

common::Result<void> MultiGuestFanout::broadcastVideoFrame(const encode::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    totalFramesBroadcast_++;

    for (auto& [guestId, entry] : guests_) {
        // 1. Connection check
        if (!entry.stats.isConnected) {
            continue;
        }
        if (entry.transport && !entry.transport->isConnected()) {
            continue;
        }

        // 2. Slow-client isolation check:
        // Bound latency by dropping frame if guest queue depth >= maxQueueDepth.
        // A congested guest never blocks or delays healthy peers.
        if (entry.stats.simulatedQueueDepth >= entry.stats.maxQueueDepth) {
            entry.stats.framesDropped++;
            entry.stats.consecutiveDrops++;
            if (entry.stats.consecutiveDrops >= 3) {
                entry.stats.isSlowClient = true;
            }
            continue;
        }

        // 3. Dispatch to guest
        common::Result<void> sendRes = common::Result<void>::success();
        if (entry.isCustomSender && entry.videoSender) {
            sendRes = entry.videoSender(packet);
        } else if (entry.transport) {
            sendRes = entry.transport->sendVideoPacket(packet);
        }

        if (sendRes.hasValue()) {
            entry.stats.framesSent++;
            entry.stats.bytesSent += packet.data.size();
            entry.stats.lastFrameTimestampNs = packet.timestampNs;
            entry.stats.consecutiveDrops = 0;
            entry.stats.isSlowClient = false;
        } else {
            entry.stats.framesDropped++;
            entry.stats.consecutiveDrops++;
            if (entry.stats.consecutiveDrops >= 3) {
                entry.stats.isSlowClient = true;
            }
        }
    }

    return common::Result<void>::success();
}

common::Result<void> MultiGuestFanout::broadcastAudioFrame(
    std::span<const uint8_t> opusPayload,
    uint32_t sampleCount
) {
    std::lock_guard<std::mutex> lock(mutex_);
    totalAudioBroadcast_++;

    for (auto& [guestId, entry] : guests_) {
        if (!entry.stats.isConnected) {
            continue;
        }
        if (entry.transport && !entry.transport->isConnected()) {
            continue;
        }

        common::Result<void> res = common::Result<void>::success();
        if (entry.isCustomSender && entry.audioSender) {
            res = entry.audioSender(opusPayload, sampleCount);
        } else if (entry.transport) {
            res = entry.transport->sendAudioFrame(opusPayload, sampleCount);
        }

        if (res.hasValue()) {
            entry.stats.audioFramesSent++;
        } else {
            entry.stats.audioFramesDropped++;
        }
    }

    return common::Result<void>::success();
}

void MultiGuestFanout::setKeyframeRequestCallback(GlobalKeyframeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    keyframeCb_ = std::move(cb);
}

void MultiGuestFanout::requestKeyframe(const std::string& guestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    if (hasSentKeyframe_) {
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeyframeTime_).count();
        if (elapsedMs < static_cast<int64_t>(keyframeDebounceMs_)) {
            MIST_LOG_INFO(common::SubsystemId::Transport,
                          "MultiGuestFanout: Debouncing keyframe request from guest " + guestId +
                          " (" + std::to_string(elapsedMs) + "ms since last keyframe)");
            return;
        }
    }

    hasSentKeyframe_ = true;
    lastKeyframeTime_ = now;
    MIST_LOG_INFO(common::SubsystemId::Transport,
                  "MultiGuestFanout: Firing keyframe to host encoder on request from guest " + guestId);

    if (keyframeCb_) {
        keyframeCb_();
    }
}

void MultiGuestFanout::setGuestSimulatedQueueDepth(const std::string& guestId, uint32_t depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it != guests_.end()) {
        it->second.stats.simulatedQueueDepth = depth;
    }
}

void MultiGuestFanout::setGuestConnected(const std::string& guestId, bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it != guests_.end()) {
        it->second.stats.isConnected = connected;
    }
}

common::Result<GuestEndpointStats> MultiGuestFanout::getGuestStats(const std::string& guestId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it == guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound,
                             "Guest ID not found: " + guestId);
    }
    return it->second.stats;
}

MultiGuestFanoutSnapshot MultiGuestFanout::getSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    MultiGuestFanoutSnapshot snapshot{};
    snapshot.totalGuests = guests_.size();
    snapshot.totalFramesBroadcast = totalFramesBroadcast_;
    snapshot.totalAudioBroadcast = totalAudioBroadcast_;

    for (const auto& [_, entry] : guests_) {
        if (entry.stats.isConnected) {
            snapshot.activeGuests++;
        }
        if (entry.stats.isSlowClient) {
            snapshot.slowGuests++;
        }
        snapshot.totalFramesDelivered += entry.stats.framesSent;
        snapshot.totalFramesDropped += entry.stats.framesDropped;
        snapshot.guestStats.push_back(entry.stats);
    }

    return snapshot;
}

void MultiGuestFanout::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    totalFramesBroadcast_ = 0;
    totalAudioBroadcast_ = 0;
    for (auto& [_, entry] : guests_) {
        entry.stats.framesSent = 0;
        entry.stats.framesDropped = 0;
        entry.stats.bytesSent = 0;
        entry.stats.audioFramesSent = 0;
        entry.stats.audioFramesDropped = 0;
        entry.stats.consecutiveDrops = 0;
        entry.stats.isSlowClient = false;
    }
}

} // namespace mist::transport
