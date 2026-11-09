#pragma once

#include "mist/transport/LowLatencyTuning.h"
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include <optional>
#include <atomic>

namespace mist::transport {

struct PacedPacket {
    std::vector<uint8_t> data;
    uint64_t enqueueTimeNs{0};
    bool isHighPriority{false};
};

class PacedSender {
public:
    explicit PacedSender(const LowLatencyProfile& profile = {}) noexcept;
    ~PacedSender() = default;

    void setPacingRate(uint32_t targetBitrateKbps) noexcept;
    void updateProfile(const LowLatencyProfile& profile) noexcept;

    [[nodiscard]] bool enqueuePacket(
        std::vector<uint8_t> data,
        uint64_t enqueueTimeNs,
        bool isHighPriority = false
    );

    // Pops the next packet eligible for transmission based on token bucket leak and pacing interval.
    // If nowNs exceeds maxQueueDurationMs for stale video packets, drops them and returns the fresher packet.
    [[nodiscard]] std::optional<std::vector<uint8_t>> getNextPacketToTransmit(uint64_t nowNs) noexcept;

    [[nodiscard]] size_t getQueueSize() const noexcept;
    [[nodiscard]] uint64_t getPacketsSent() const noexcept { return packetsSent_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getPacketsDropped() const noexcept { return packetsDropped_.load(std::memory_order_relaxed); }

    void clear() noexcept;

private:
    mutable std::mutex mutex_;
    LowLatencyProfile profile_;
    uint32_t targetBitrateKbps_{15000};
    double tokenBucketBytes_{0.0};
    double maxTokenBucketBytes_{65536.0};
    uint64_t lastTokenUpdateTimeNs_{0};

    std::queue<PacedPacket> highPriorityQueue_;
    std::queue<PacedPacket> videoQueue_;

    std::atomic<uint64_t> packetsSent_{0};
    std::atomic<uint64_t> packetsDropped_{0};
};

} // namespace mist::transport
