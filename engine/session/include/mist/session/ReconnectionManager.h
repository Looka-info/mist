#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <mutex>
#include <functional>
#include <string_view>
#include <chrono>

namespace mist::session {

enum class ReconnectionStatus : uint8_t {
    Idle = 0,
    Reconnecting,
    WaitingRetry,
    Reconnected,
    Exhausted,
    Aborted
};

[[nodiscard]] constexpr std::string_view reconnectionStatusToString(ReconnectionStatus s) noexcept {
    switch (s) {
        case ReconnectionStatus::Idle:         return "Idle";
        case ReconnectionStatus::Reconnecting: return "Reconnecting";
        case ReconnectionStatus::WaitingRetry:  return "WaitingRetry";
        case ReconnectionStatus::Reconnected:  return "Reconnected";
        case ReconnectionStatus::Exhausted:    return "Exhausted";
        case ReconnectionStatus::Aborted:      return "Aborted";
    }
    return "Unknown";
}

struct ReconnectionConfig {
    uint32_t initialIntervalMs{500};      // Initial backoff delay (0.5s)
    uint32_t maxIntervalMs{15000};         // Maximum backoff delay cap (15s)
    float backoffMultiplier{1.5f};        // Exponential scaling multiplier
    uint32_t maxAttempts{10};              // Maximum reconnection attempts before exhausting
    float jitterFactor{0.15f};             // Random jitter (+/- 15%)
    uint32_t fastReconnectWindowMs{3000};  // If disconnected < 3s, immediate zero-delay attempt
};

using ReconnectionStateCallback = std::function<void(ReconnectionStatus status, uint32_t attempt, uint32_t nextDelayMs)>;

class ReconnectionManager {
public:
    explicit ReconnectionManager(ReconnectionConfig config = {}) noexcept;
    ~ReconnectionManager() = default;

    // Triggered when connection drops
    // Returns delay in milliseconds before attempting reconnection
    uint32_t startReconnecting(uint64_t disconnectedTimestampNs = 0);

    // Call when an attempt fails. Returns next delay in ms, or error if exhausted
    common::Result<uint32_t> recordAttemptFailure();

    // Call when reconnection succeeds
    void recordSuccess();

    // Abort reconnecting (e.g. user cancelled)
    void abort();

    // Reset to idle state
    void reset();

    // Compute expected backoff delay for a given attempt index without mutating state
    [[nodiscard]] uint32_t computeBackoffDelay(uint32_t attemptIndex) const noexcept;

    // Status queries
    [[nodiscard]] ReconnectionStatus getStatus() const noexcept;
    [[nodiscard]] uint32_t getAttemptCount() const noexcept;
    [[nodiscard]] uint32_t getMaxAttempts() const noexcept;
    [[nodiscard]] bool isReconnecting() const noexcept;

    void setStateCallback(ReconnectionStateCallback cb);

private:
    void setStatusUnderLock(ReconnectionStatus status, uint32_t delayMs);

    mutable std::mutex mutex_;
    ReconnectionConfig config_;
    ReconnectionStatus status_{ReconnectionStatus::Idle};
    uint32_t attemptCount_{0};
    uint64_t disconnectTimeNs_{0};
    ReconnectionStateCallback stateCb_;
};

} // namespace mist::session
