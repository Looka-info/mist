#include "mist/transport/NackPliController.h"

namespace mist::transport {

NackPliController::NackPliController(const LowLatencyProfile& profile) noexcept
    : profile_(profile) {}

void NackPliController::updateProfile(const LowLatencyProfile& profile) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    profile_ = profile;
}

NackPliDecision NackPliController::onRtpPacketReceived(
    uint16_t seqNumber,
    uint32_t currentRttMs,
    uint64_t nowNs
) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    NackPliDecision decision{};

    if (!hasReceivedFirstPacket_) {
        hasReceivedFirstPacket_ = true;
        highestSeq_ = seqNumber;
        return decision;
    }

    int16_t diff = static_cast<int16_t>(seqNumber - highestSeq_);
    if (diff <= 0) {
        // Duplicate or late out-of-order packet
        return decision;
    }

    if (diff == 1) {
        // Normal consecutive packet
        highestSeq_ = seqNumber;
        return decision;
    }

    // Gap detected: sequence numbers (highestSeq_ + 1) to (seqNumber - 1)
    uint16_t missingCount = static_cast<uint16_t>(diff - 1);
    for (uint16_t i = 1; i < static_cast<uint16_t>(diff); ++i) {
        decision.missingSequenceNumbers.push_back(static_cast<uint16_t>(highestSeq_ + i));
    }
    highestSeq_ = seqNumber;

    if (profile_.enableFastNack && currentRttMs <= profile_.nackRttThresholdMs) {
        // Fast NACK: RTT is low enough that retransmission will arrive before next frame display
        decision.shouldSendNack = true;
        nacksGenerated_.fetch_add(missingCount, std::memory_order_relaxed);
        decision.rationale = "Low RTT (" + std::to_string(currentRttMs) + "ms <= " +
                             std::to_string(profile_.nackRttThresholdMs) + "ms); fast NACK requested";
    } else {
        // High RTT: NACK will arrive too late, causing stutter. Suppress NACK and escalate to debounced PLI
        nacksSuppressed_.fetch_add(missingCount, std::memory_order_relaxed);
        uint64_t debounceNs = static_cast<uint64_t>(profile_.pliDebounceIntervalMs) * 1000000ULL;
        if (lastPliTimeNs_ == 0 || (nowNs - lastPliTimeNs_) >= debounceNs) {
            lastPliTimeNs_ = nowNs;
            plisRequested_.fetch_add(1, std::memory_order_relaxed);
            decision.shouldSendPli = true;
            decision.rationale = "High RTT (" + std::to_string(currentRttMs) + "ms > " +
                                 std::to_string(profile_.nackRttThresholdMs) + "ms); NACK suppressed, PLI dispatched";
        } else {
            plisSuppressed_.fetch_add(1, std::memory_order_relaxed);
            decision.shouldSendPli = false;
            decision.rationale = "High RTT (" + std::to_string(currentRttMs) + "ms); NACK suppressed, PLI debounced";
        }
    }

    return decision;
}

bool NackPliController::requestPli(uint64_t nowNs) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t debounceNs = static_cast<uint64_t>(profile_.pliDebounceIntervalMs) * 1000000ULL;
    if (lastPliTimeNs_ == 0 || (nowNs - lastPliTimeNs_) >= debounceNs) {
        lastPliTimeNs_ = nowNs;
        plisRequested_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    plisSuppressed_.fetch_add(1, std::memory_order_relaxed);
    return false;
}

void NackPliController::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    hasReceivedFirstPacket_ = false;
    highestSeq_ = 0;
    lastPliTimeNs_ = 0;
}

} // namespace mist::transport
