#include "mist/transport/PacedSender.h"
#include <algorithm>

namespace mist::transport {

PacedSender::PacedSender(const LowLatencyProfile& profile) noexcept
    : profile_(profile) {
    setPacingRate(15000);
}

void PacedSender::setPacingRate(uint32_t targetBitrateKbps) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    targetBitrateKbps_ = std::max(500u, targetBitrateKbps);

    // Compute token bucket burst size (~10 ms worth of data)
    double bytesPerSec = (static_cast<double>(targetBitrateKbps_) * 1000.0 * profile_.pacingFactor) / 8.0;
    maxTokenBucketBytes_ = std::max(16384.0, bytesPerSec * 0.010);
    tokenBucketBytes_ = maxTokenBucketBytes_;
}

void PacedSender::updateProfile(const LowLatencyProfile& profile) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    profile_ = profile;
    setPacingRate(targetBitrateKbps_);
}

bool PacedSender::enqueuePacket(
    std::vector<uint8_t> data,
    uint64_t enqueueTimeNs,
    bool isHighPriority
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (data.empty()) return false;

    // Bound total queue size
    if (highPriorityQueue_.size() + videoQueue_.size() >= profile_.maxQueuePackets) {
        if (!videoQueue_.empty()) {
            videoQueue_.pop();
            packetsDropped_.fetch_add(1, std::memory_order_relaxed);
        } else {
            packetsDropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    PacedPacket pkt{
        .data = std::move(data),
        .enqueueTimeNs = enqueueTimeNs,
        .isHighPriority = isHighPriority
    };

    if (isHighPriority) {
        highPriorityQueue_.push(std::move(pkt));
    } else {
        videoQueue_.push(std::move(pkt));
    }

    return true;
}

std::optional<std::vector<uint8_t>> PacedSender::getNextPacketToTransmit(uint64_t nowNs) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. High priority (audio/control/RTCP) always takes precedence with zero pacing delay
    if (!highPriorityQueue_.empty()) {
        auto pkt = std::move(highPriorityQueue_.front());
        highPriorityQueue_.pop();
        packetsSent_.fetch_add(1, std::memory_order_relaxed);
        return pkt.data;
    }

    if (videoQueue_.empty()) {
        return std::nullopt;
    }

    // 2. Replenish token bucket based on elapsed time
    if (lastTokenUpdateTimeNs_ == 0) {
        lastTokenUpdateTimeNs_ = nowNs;
    }

    if (nowNs > lastTokenUpdateTimeNs_) {
        double elapsedSec = static_cast<double>(nowNs - lastTokenUpdateTimeNs_) / 1000000000.0;
        double bytesPerSec = (static_cast<double>(targetBitrateKbps_) * 1000.0 * profile_.pacingFactor) / 8.0;
        tokenBucketBytes_ = std::min(maxTokenBucketBytes_, tokenBucketBytes_ + (elapsedSec * bytesPerSec));
        lastTokenUpdateTimeNs_ = nowNs;
    }

    // 3. Drop stale video packets exceeding maxQueueDurationMs
    const uint64_t maxQueueDurationNs = static_cast<uint64_t>(profile_.maxQueueDurationMs) * 1000000ULL;
    while (!videoQueue_.empty()) {
        const auto& front = videoQueue_.front();
        if (nowNs >= front.enqueueTimeNs && (nowNs - front.enqueueTimeNs) > maxQueueDurationNs) {
            videoQueue_.pop();
            packetsDropped_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        break;
    }

    if (videoQueue_.empty()) {
        return std::nullopt;
    }

    // 4. Check if we have enough tokens to transmit the front packet
    const size_t packetSize = videoQueue_.front().data.size();
    if (tokenBucketBytes_ >= static_cast<double>(packetSize) || !profile_.enablePacedSender) {
        tokenBucketBytes_ -= static_cast<double>(packetSize);
        auto pkt = std::move(videoQueue_.front());
        videoQueue_.pop();
        packetsSent_.fetch_add(1, std::memory_order_relaxed);
        return pkt.data;
    }

    return std::nullopt; // Pacer holding burst to smooth out delivery
}

size_t PacedSender::getQueueSize() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return highPriorityQueue_.size() + videoQueue_.size();
}

void PacedSender::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!highPriorityQueue_.empty()) highPriorityQueue_.pop();
    while (!videoQueue_.empty()) videoQueue_.pop();
    lastTokenUpdateTimeNs_ = 0;
}

} // namespace mist::transport
