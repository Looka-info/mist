#include "mist/transport/TransportStatistics.h"
#include <mutex>

namespace mist::transport {

static std::mutex g_statsMutex;

void TransportStatistics::recordBytesSent(uint64_t bytes) noexcept {
    bytesSent_.fetch_add(bytes, std::memory_order_relaxed);
}

void TransportStatistics::recordBytesReceived(uint64_t bytes) noexcept {
    bytesReceived_.fetch_add(bytes, std::memory_order_relaxed);
}

void TransportStatistics::recordPacketSent() noexcept {
    packetsSent_.fetch_add(1, std::memory_order_relaxed);
}

void TransportStatistics::recordPacketReceived() noexcept {
    packetsReceived_.fetch_add(1, std::memory_order_relaxed);
}

void TransportStatistics::recordPacketLost(uint64_t count) noexcept {
    packetsLost_.fetch_add(count, std::memory_order_relaxed);
}

void TransportStatistics::updateRtt(double rttMs) noexcept {
    rttMs_.store(rttMs, std::memory_order_relaxed);
}

void TransportStatistics::updateJitter(double jitterMs) noexcept {
    jitterMs_.store(jitterMs, std::memory_order_relaxed);
}

void TransportStatistics::updateDeliveryLatency(double latencyMs) noexcept {
    deliveryLatencyMs_.store(latencyMs, std::memory_order_relaxed);
}

void TransportStatistics::setCandidateType(std::string type) noexcept {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    candidateType_ = std::move(type);
}

void TransportStatistics::setTransportState(std::string state) noexcept {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    transportState_ = std::move(state);
}

TransportStatsSnapshot TransportStatistics::getSnapshot() const noexcept {
    TransportStatsSnapshot snap{};
    snap.bytesSent = bytesSent_.load(std::memory_order_relaxed);
    snap.bytesReceived = bytesReceived_.load(std::memory_order_relaxed);
    snap.packetsSent = packetsSent_.load(std::memory_order_relaxed);
    snap.packetsReceived = packetsReceived_.load(std::memory_order_relaxed);
    snap.packetsLost = packetsLost_.load(std::memory_order_relaxed);
    snap.rttMs = rttMs_.load(std::memory_order_relaxed);
    snap.jitterMs = jitterMs_.load(std::memory_order_relaxed);
    snap.deliveryLatencyMs = deliveryLatencyMs_.load(std::memory_order_relaxed);

    uint64_t total = snap.packetsReceived + snap.packetsLost;
    if (total > 0) {
        snap.packetLossPercent = (static_cast<double>(snap.packetsLost) / static_cast<double>(total)) * 100.0;
    }

    {
        std::lock_guard<std::mutex> lock(g_statsMutex);
        snap.candidateType = candidateType_;
        snap.transportState = transportState_;
    }

    return snap;
}

void TransportStatistics::reset() noexcept {
    bytesSent_.store(0, std::memory_order_relaxed);
    bytesReceived_.store(0, std::memory_order_relaxed);
    packetsSent_.store(0, std::memory_order_relaxed);
    packetsReceived_.store(0, std::memory_order_relaxed);
    packetsLost_.store(0, std::memory_order_relaxed);
    rttMs_.store(0.0, std::memory_order_relaxed);
    jitterMs_.store(0.0, std::memory_order_relaxed);
    deliveryLatencyMs_.store(0.0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(g_statsMutex);
        candidateType_ = "unknown";
        transportState_ = "new";
    }
}

} // namespace mist::transport
