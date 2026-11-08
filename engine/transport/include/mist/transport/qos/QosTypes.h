#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>

namespace mist::transport::qos {

enum class CongestionState : uint8_t {
    Stable = 0,
    Increasing,
    Congested,
    Recovering,
    CriticallyCongested
};

[[nodiscard]] constexpr std::string_view congestionStateToString(CongestionState state) noexcept {
    switch (state) {
        case CongestionState::Stable: return "Stable";
        case CongestionState::Increasing: return "Increasing";
        case CongestionState::Congested: return "Congested";
        case CongestionState::Recovering: return "Recovering";
        case CongestionState::CriticallyCongested: return "CriticallyCongested";
        default: return "Unknown";
    }
}

enum class QualityProfile : uint8_t {
    Performance = 0,
    Balanced,
    Quality,
    Adaptive
};

[[nodiscard]] constexpr std::string_view qualityProfileToString(QualityProfile profile) noexcept {
    switch (profile) {
        case QualityProfile::Performance: return "Performance";
        case QualityProfile::Balanced: return "Balanced";
        case QualityProfile::Quality: return "Quality";
        case QualityProfile::Adaptive: return "Adaptive";
        default: return "Unknown";
    }
}

struct RtpStatistics {
    uint64_t packetsSent{0};
    uint64_t packetsReceived{0};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    uint32_t bitrateBps{0};
    uint32_t packetsLost{0};
    uint64_t timestampUs{0};
};

struct RtcpStatistics {
    uint8_t fractionLost{0};           // 0..255 (fixed point 1/256th)
    uint32_t cumulativePacketLoss{0};
    uint32_t jitterSamples{0};
    double roundTripTimeMs{0.0};
    uint32_t lastSrDelayMs{0};
    uint32_t receiverEstimatedMaxBitrateBps{0}; // From REMB
    uint64_t timestampUs{0};

    [[nodiscard]] double lossPercent() const noexcept {
        return (static_cast<double>(fractionLost) / 256.0) * 100.0;
    }
};

struct ProfileConfig {
    QualityProfile profile{QualityProfile::Adaptive};
    uint32_t minBitrateKbps{2000};
    uint32_t maxBitrateKbps{20000};
    uint32_t initialBitrateKbps{8000};
    uint32_t targetFps{60};
    uint32_t width{1920};
    uint32_t height{1080};
    double maxAcceptableRttMs{60.0};
    double lossThresholdPercent{2.5};
};

struct BandwidthBudget {
    uint32_t totalBandwidthKbps{8000};
    uint32_t videoBitrateKbps{7500};
    uint32_t audioBitrateKbps{128};
    uint32_t controlBitrateKbps{50};
    uint32_t transportHeadroomKbps{322};
};

enum class ConnectionType : uint8_t {
    Unknown = 0,
    DirectLan,
    DirectWan,
    RelayedTurn
};

[[nodiscard]] constexpr std::string_view connectionTypeToString(ConnectionType type) noexcept {
    switch (type) {
        case ConnectionType::DirectLan: return "Direct (LAN)";
        case ConnectionType::DirectWan: return "Direct (WAN)";
        case ConnectionType::RelayedTurn: return "Relayed (TURN)";
        default: return "Unknown";
    }
}

} // namespace mist::transport::qos
