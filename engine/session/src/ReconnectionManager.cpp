#include "mist/session/ReconnectionManager.h"
#include <algorithm>
#include <cmath>

namespace mist::session {

namespace {

uint64_t getNowNs() noexcept {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace

ReconnectionManager::ReconnectionManager(ReconnectionConfig config) noexcept
    : config_(config) {}

uint32_t ReconnectionManager::startReconnecting(uint64_t disconnectedTimestampNs) {
    std::lock_guard<std::mutex> lock(mutex_);

    attemptCount_ = 0;
    disconnectTimeNs_ = (disconnectedTimestampNs > 0) ? disconnectedTimestampNs : getNowNs();

    uint64_t nowNs = getNowNs();
    uint64_t elapsedNs = (nowNs > disconnectTimeNs_) ? (nowNs - disconnectTimeNs_) : 0;
    uint64_t fastWindowNs = static_cast<uint64_t>(config_.fastReconnectWindowMs) * 1000000ULL;

    uint32_t initialDelayMs = 0;
    if (elapsedNs > fastWindowNs) {
        // Exceeded fast reconnect window, use normal initial backoff
        initialDelayMs = computeBackoffDelay(0);
    } else {
        // Immediate zero-delay attempt for transient drops
        initialDelayMs = 0;
    }

    setStatusUnderLock(ReconnectionStatus::Reconnecting, initialDelayMs);
    return initialDelayMs;
}

common::Result<uint32_t> ReconnectionManager::recordAttemptFailure() {
    std::lock_guard<std::mutex> lock(mutex_);

    attemptCount_++;

    if (attemptCount_ >= config_.maxAttempts) {
        setStatusUnderLock(ReconnectionStatus::Exhausted, 0);
        return common::Error(common::SubsystemId::Session, common::ErrorCode::ResourceExhausted,
                             "Reconnection attempts exhausted after " + std::to_string(attemptCount_) + " tries");
    }

    uint32_t nextDelayMs = computeBackoffDelay(attemptCount_);
    setStatusUnderLock(ReconnectionStatus::WaitingRetry, nextDelayMs);
    return nextDelayMs;
}

void ReconnectionManager::recordSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);
    attemptCount_ = 0;
    setStatusUnderLock(ReconnectionStatus::Reconnected, 0);
}

void ReconnectionManager::abort() {
    std::lock_guard<std::mutex> lock(mutex_);
    setStatusUnderLock(ReconnectionStatus::Aborted, 0);
}

void ReconnectionManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    attemptCount_ = 0;
    disconnectTimeNs_ = 0;
    setStatusUnderLock(ReconnectionStatus::Idle, 0);
}

uint32_t ReconnectionManager::computeBackoffDelay(uint32_t attemptIndex) const noexcept {
    double baseDelay = static_cast<double>(config_.initialIntervalMs) *
                       std::pow(static_cast<double>(config_.backoffMultiplier), static_cast<double>(attemptIndex));

    double clampedDelay = std::min(baseDelay, static_cast<double>(config_.maxIntervalMs));

    // Jitter: +/- jitterFactor applied deterministically based on attempt to avoid synchronized thundering herd
    double jitterRange = clampedDelay * static_cast<double>(config_.jitterFactor);
    double jitterOffset = ((static_cast<double>((attemptIndex * 179424673ULL) % 1000) / 500.0) - 1.0) * jitterRange;
    double finalDelay = std::max(10.0, clampedDelay + jitterOffset);

    return static_cast<uint32_t>(std::round(finalDelay));
}

ReconnectionStatus ReconnectionManager::getStatus() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

uint32_t ReconnectionManager::getAttemptCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return attemptCount_;
}

uint32_t ReconnectionManager::getMaxAttempts() const noexcept {
    return config_.maxAttempts;
}

bool ReconnectionManager::isReconnecting() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_ == ReconnectionStatus::Reconnecting || status_ == ReconnectionStatus::WaitingRetry;
}

void ReconnectionManager::setStateCallback(ReconnectionStateCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateCb_ = std::move(cb);
}

void ReconnectionManager::setStatusUnderLock(ReconnectionStatus status, uint32_t delayMs) {
    status_ = status;
    if (stateCb_) {
        stateCb_(status_, attemptCount_, delayMs);
    }
}

} // namespace mist::session
