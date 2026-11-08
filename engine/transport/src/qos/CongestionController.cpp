#include "mist/transport/qos/CongestionController.h"
#include <algorithm>

namespace mist::transport::qos {

CongestionController::CongestionController(const ProfileConfig& config) noexcept
    : config_(config),
      currentBitrateKbps_(config.initialBitrateKbps > 0 ? config.initialBitrateKbps : 8000) {}

void CongestionController::updateProfileConfig(const ProfileConfig& config) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    currentBitrateKbps_ = std::clamp(currentBitrateKbps_, config_.minBitrateKbps, config_.maxBitrateKbps);
}

void CongestionController::transitionTo(CongestionState newState, uint64_t nowMs) noexcept {
    if (state_ != newState) {
        state_ = newState;
        lastStateChangeMs_ = nowMs;
    }
}

uint32_t CongestionController::processFeedback(
    double smoothedRttMs,
    double smoothedLossPercent,
    [[maybe_unused]] double smoothedJitterMs,
    uint32_t estimatedBandwidthBps,
    uint64_t currentEpochMs
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (lastStateChangeMs_ == 0) {
        lastStateChangeMs_ = currentEpochMs;
    }

    uint64_t timeSinceLastBackoff = (lastBackoffMs_ > 0) ? (currentEpochMs - lastBackoffMs_) : kMinBackoffCooldownMs;
    uint64_t timeSinceLastAdjustment = (lastAdjustmentMs_ > 0) ? (currentEpochMs - lastAdjustmentMs_) : kIncreaseIntervalMs;

    bool isSevereLoss = (smoothedLossPercent >= 15.0);
    bool isSevereRtt = (smoothedRttMs > config_.maxAcceptableRttMs * 2.5);
    bool isCongestedLoss = (smoothedLossPercent >= config_.lossThresholdPercent);
    bool isCongestedRtt = (smoothedRttMs > config_.maxAcceptableRttMs);

    // 1. Critical Congestion: immediate aggressive multiplicative decrease
    if (isSevereLoss || isSevereRtt) {
        if (timeSinceLastBackoff >= (kMinBackoffCooldownMs / 2)) {
            transitionTo(CongestionState::CriticallyCongested, currentEpochMs);
            // 50% multiplicative cut, clamped to minimum floor
            currentBitrateKbps_ = std::max(config_.minBitrateKbps, currentBitrateKbps_ / 2);
            lastBackoffMs_ = currentEpochMs;
            lastAdjustmentMs_ = currentEpochMs;
        }
        return currentBitrateKbps_;
    }

    // 2. Persistent Congestion: moderate multiplicative decrease with cooldown protection
    if (isCongestedLoss || isCongestedRtt) {
        if (timeSinceLastBackoff >= kMinBackoffCooldownMs) {
            transitionTo(CongestionState::Congested, currentEpochMs);
            // 20% multiplicative decrease (x0.80)
            uint32_t reduced = static_cast<uint32_t>(static_cast<double>(currentBitrateKbps_) * 0.80);
            currentBitrateKbps_ = std::max(config_.minBitrateKbps, reduced);
            lastBackoffMs_ = currentEpochMs;
            lastAdjustmentMs_ = currentEpochMs;
        }
        return currentBitrateKbps_;
    }

    // 3. Post-Congestion Recovery: hold steady until network proves stable
    if (state_ == CongestionState::Congested || state_ == CongestionState::CriticallyCongested) {
        transitionTo(CongestionState::Recovering, currentEpochMs);
        return currentBitrateKbps_;
    }

    if (state_ == CongestionState::Recovering) {
        if (currentEpochMs - lastStateChangeMs_ >= kRecoveryHoldMs) {
            transitionTo(CongestionState::Stable, currentEpochMs);
        }
        return currentBitrateKbps_;
    }

    // 4. Bandwidth Probing / Additive Increase
    if (state_ == CongestionState::Stable || state_ == CongestionState::Increasing) {
        if (timeSinceLastAdjustment >= kIncreaseIntervalMs) {
            transitionTo(CongestionState::Increasing, currentEpochMs);

            // Additive step: +5% or +250 kbps, whichever is larger
            uint32_t step = std::max(250U, currentBitrateKbps_ / 20);
            uint32_t candidateBitrate = currentBitrateKbps_ + step;

            // Cap candidate to estimated bandwidth if REMB is available and higher than min
            if (estimatedBandwidthBps > 0) {
                uint32_t rembKbps = estimatedBandwidthBps / 1000;
                if (rembKbps > config_.minBitrateKbps) {
                    candidateBitrate = std::min(candidateBitrate, rembKbps);
                }
            }

            currentBitrateKbps_ = std::min(config_.maxBitrateKbps, candidateBitrate);
            lastAdjustmentMs_ = currentEpochMs;
        }
    }

    return currentBitrateKbps_;
}

CongestionState CongestionController::getState() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

uint32_t CongestionController::getCurrentBitrateKbps() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentBitrateKbps_;
}

uint64_t CongestionController::getLastStateChangeTimeMs() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastStateChangeMs_;
}

void CongestionController::reset(uint32_t initialBitrateKbps) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CongestionState::Stable;
    if (initialBitrateKbps > 0) {
        currentBitrateKbps_ = std::clamp(initialBitrateKbps, config_.minBitrateKbps, config_.maxBitrateKbps);
    } else {
        currentBitrateKbps_ = config_.initialBitrateKbps;
    }
    lastStateChangeMs_ = 0;
    lastAdjustmentMs_ = 0;
    lastBackoffMs_ = 0;
}

} // namespace mist::transport::qos
