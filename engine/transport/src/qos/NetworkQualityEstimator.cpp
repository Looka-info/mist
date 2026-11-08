#include "mist/transport/qos/NetworkQualityEstimator.h"
#include <algorithm>
#include <cmath>

namespace mist::transport::qos {

NetworkQualityEstimator::NetworkQualityEstimator() noexcept = default;

void NetworkQualityEstimator::updateRtcpStats(const RtcpStatistics& stats) {
    std::lock_guard<std::mutex> lock(mutex_);
    latestRtcp_ = stats;

    double lossPercent = stats.lossPercent();
    if (!hasLossSample_) {
        smoothedLossPercent_ = lossPercent;
        hasLossSample_ = true;
    } else {
        // EWMA with alpha = 0.20
        smoothedLossPercent_ = 0.80 * smoothedLossPercent_ + 0.20 * lossPercent;
    }

    if (stats.roundTripTimeMs > 0.0) {
        if (!hasRttSample_) {
            smoothedRttMs_ = stats.roundTripTimeMs;
            hasRttSample_ = true;
        } else {
            // EWMA with alpha = 0.15
            smoothedRttMs_ = 0.85 * smoothedRttMs_ + 0.15 * stats.roundTripTimeMs;
        }
    }

    if (stats.jitterSamples > 0) {
        // Assuming 90 kHz video clock, convert samples to ms
        double jitterMs = static_cast<double>(stats.jitterSamples) / 90.0;
        smoothedJitterMs_ = 0.85 * smoothedJitterMs_ + 0.15 * jitterMs;
    }

    if (stats.receiverEstimatedMaxBitrateBps > 0) {
        hasRembSample_ = true;
        estimatedBandwidthBps_ = stats.receiverEstimatedMaxBitrateBps;
    }
}

void NetworkQualityEstimator::updateRtpStats(const RtpStatistics& stats) {
    std::lock_guard<std::mutex> lock(mutex_);
    latestRtp_ = stats;
    if (stats.bitrateBps > 0 && !hasRembSample_) {
        // If no REMB is present, track delivered throughput
        estimatedBandwidthBps_ = std::max(estimatedBandwidthBps_, stats.bitrateBps);
    }
}

void NetworkQualityEstimator::updateRembBitrate(uint32_t bitrateBps) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bitrateBps > 0) {
        hasRembSample_ = true;
        latestRtcp_.receiverEstimatedMaxBitrateBps = bitrateBps;
        estimatedBandwidthBps_ = bitrateBps;
    }
}

void NetworkQualityEstimator::updateSelectedCandidatePair(
    const std::string& localCandidateType,
    const std::string& remoteCandidateType
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (localCandidateType == "relay" || remoteCandidateType == "relay" ||
        localCandidateType == "relayed" || remoteCandidateType == "relayed") {
        connectionType_ = ConnectionType::RelayedTurn;
    } else if (localCandidateType == "host" && remoteCandidateType == "host") {
        connectionType_ = ConnectionType::DirectLan;
    } else {
        connectionType_ = ConnectionType::DirectWan;
    }
}

double NetworkQualityEstimator::getSmoothedRttMs() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return smoothedRttMs_;
}

double NetworkQualityEstimator::getSmoothedLossPercent() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return smoothedLossPercent_;
}

double NetworkQualityEstimator::getSmoothedJitterMs() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return smoothedJitterMs_;
}

uint32_t NetworkQualityEstimator::getEstimatedBandwidthBps() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return estimatedBandwidthBps_;
}

ConnectionType NetworkQualityEstimator::getConnectionType() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return connectionType_;
}

RtcpStatistics NetworkQualityEstimator::getLatestRtcpStats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestRtcp_;
}

RtpStatistics NetworkQualityEstimator::getLatestRtpStats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestRtp_;
}

void NetworkQualityEstimator::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    latestRtcp_ = {};
    latestRtp_ = {};
    smoothedRttMs_ = 10.0;
    smoothedLossPercent_ = 0.0;
    smoothedJitterMs_ = 1.0;
    estimatedBandwidthBps_ = 8000000;
    connectionType_ = ConnectionType::Unknown;
    hasRttSample_ = false;
    hasLossSample_ = false;
    hasRembSample_ = false;
}

} // namespace mist::transport::qos
