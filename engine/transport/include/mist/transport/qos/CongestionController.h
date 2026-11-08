#pragma once

#include "QosTypes.h"
#include <mutex>
#include <chrono>

namespace mist::transport::qos {

class CongestionController {
public:
    explicit CongestionController(const ProfileConfig& config = {}) noexcept;
    ~CongestionController() = default;

    void updateProfileConfig(const ProfileConfig& config) noexcept;

    // Evaluates current network metrics and computes next bitrate target
    [[nodiscard]] uint32_t processFeedback(
        double smoothedRttMs,
        double smoothedLossPercent,
        double smoothedJitterMs,
        uint32_t estimatedBandwidthBps,
        uint64_t currentEpochMs
    );

    [[nodiscard]] CongestionState getState() const noexcept;
    [[nodiscard]] uint32_t getCurrentBitrateKbps() const noexcept;
    [[nodiscard]] uint64_t getLastStateChangeTimeMs() const noexcept;

    void reset(uint32_t initialBitrateKbps = 0) noexcept;

private:
    void transitionTo(CongestionState newState, uint64_t nowMs) noexcept;

    mutable std::mutex mutex_;
    ProfileConfig config_;
    CongestionState state_{CongestionState::Stable};
    uint32_t currentBitrateKbps_{8000};

    uint64_t lastStateChangeMs_{0};
    uint64_t lastAdjustmentMs_{0};
    uint64_t lastBackoffMs_{0};

    // Cooldown and timing constants
    static constexpr uint64_t kMinBackoffCooldownMs = 1000;
    static constexpr uint64_t kIncreaseIntervalMs = 500;
    static constexpr uint64_t kRecoveryHoldMs = 2000;
};

} // namespace mist::transport::qos
