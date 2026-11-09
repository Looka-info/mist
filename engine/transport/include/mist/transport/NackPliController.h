#pragma once

#include "mist/transport/LowLatencyTuning.h"
#include <vector>
#include <mutex>
#include <cstdint>
#include <atomic>

namespace mist::transport {

struct NackPliDecision {
    bool shouldSendNack{false};
    std::vector<uint16_t> missingSequenceNumbers;
    bool shouldSendPli{false};
    std::string rationale;
};

class NackPliController {
public:
    explicit NackPliController(const LowLatencyProfile& profile = {}) noexcept;
    ~NackPliController() = default;

    void updateProfile(const LowLatencyProfile& profile) noexcept;

    // Called on every incoming RTP packet sequence number
    [[nodiscard]] NackPliDecision onRtpPacketReceived(
        uint16_t seqNumber,
        uint32_t currentRttMs,
        uint64_t nowNs
    ) noexcept;

    // Explicitly triggers a PLI request (e.g. from decoder bitstream error) with debounce protection
    [[nodiscard]] bool requestPli(uint64_t nowNs) noexcept;

    [[nodiscard]] uint64_t getNacksGenerated() const noexcept { return nacksGenerated_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getNacksSuppressed() const noexcept { return nacksSuppressed_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getPlisRequested() const noexcept { return plisRequested_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getPlisSuppressed() const noexcept { return plisSuppressed_.load(std::memory_order_relaxed); }

    void reset() noexcept;

private:
    mutable std::mutex mutex_;
    LowLatencyProfile profile_;
    bool hasReceivedFirstPacket_{false};
    uint16_t highestSeq_{0};
    uint64_t lastPliTimeNs_{0};

    std::atomic<uint64_t> nacksGenerated_{0};
    std::atomic<uint64_t> nacksSuppressed_{0};
    std::atomic<uint64_t> plisRequested_{0};
    std::atomic<uint64_t> plisSuppressed_{0};
};

} // namespace mist::transport
