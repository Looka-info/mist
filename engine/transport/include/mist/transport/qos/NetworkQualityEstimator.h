#pragma once

#include "QosTypes.h"
#include <mutex>
#include <string>

namespace mist::transport::qos {

class NetworkQualityEstimator {
public:
    NetworkQualityEstimator() noexcept;
    ~NetworkQualityEstimator() = default;

    void updateRtcpStats(const RtcpStatistics& stats);
    void updateRtpStats(const RtpStatistics& stats);
    void updateRembBitrate(uint32_t bitrateBps);
    void updateSelectedCandidatePair(const std::string& localCandidateType, const std::string& remoteCandidateType);

    [[nodiscard]] double getSmoothedRttMs() const noexcept;
    [[nodiscard]] double getSmoothedLossPercent() const noexcept;
    [[nodiscard]] double getSmoothedJitterMs() const noexcept;
    [[nodiscard]] uint32_t getEstimatedBandwidthBps() const noexcept;
    [[nodiscard]] ConnectionType getConnectionType() const noexcept;

    [[nodiscard]] RtcpStatistics getLatestRtcpStats() const noexcept;
    [[nodiscard]] RtpStatistics getLatestRtpStats() const noexcept;

    void reset() noexcept;

private:
    mutable std::mutex mutex_;
    RtcpStatistics latestRtcp_{};
    RtpStatistics latestRtp_{};

    double smoothedRttMs_{10.0};
    double smoothedLossPercent_{0.0};
    double smoothedJitterMs_{1.0};
    uint32_t estimatedBandwidthBps_{8000000}; // Default 8 Mbps
    ConnectionType connectionType_{ConnectionType::Unknown};

    bool hasRttSample_{false};
    bool hasLossSample_{false};
    bool hasRembSample_{false};
};

} // namespace mist::transport::qos
