#pragma once

#include <cstdint>
#include <string>
#include <atomic>

namespace mist::transport {

struct TransportStatsSnapshot {
    double rttMs{0.0};
    uint64_t bytesSent{0};
    uint64_t bytesReceived{0};
    uint64_t packetsSent{0};
    uint64_t packetsReceived{0};
    uint64_t packetsLost{0};
    double packetLossPercent{0.0};
    double jitterMs{0.0};
    double bitrateKbps{0.0};
    double deliveryLatencyMs{0.0};
    std::string candidateType{"unknown"};
    std::string transportState{"new"};
};

class TransportStatistics {
public:
    TransportStatistics() noexcept = default;
    ~TransportStatistics() = default;

    void recordBytesSent(uint64_t bytes) noexcept;
    void recordBytesReceived(uint64_t bytes) noexcept;
    void recordPacketSent() noexcept;
    void recordPacketReceived() noexcept;
    void recordPacketLost(uint64_t count = 1) noexcept;
    void updateRtt(double rttMs) noexcept;
    void updateJitter(double jitterMs) noexcept;
    void updateDeliveryLatency(double latencyMs) noexcept;
    void setCandidateType(std::string type) noexcept;
    void setTransportState(std::string state) noexcept;

    [[nodiscard]] TransportStatsSnapshot getSnapshot() const noexcept;
    void reset() noexcept;

private:
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<uint64_t> bytesReceived_{0};
    std::atomic<uint64_t> packetsSent_{0};
    std::atomic<uint64_t> packetsReceived_{0};
    std::atomic<uint64_t> packetsLost_{0};
    std::atomic<double> rttMs_{0.0};
    std::atomic<double> jitterMs_{0.0};
    std::atomic<double> deliveryLatencyMs_{0.0};
    std::string candidateType_{"unknown"};
    std::string transportState_{"new"};
};

} // namespace mist::transport
