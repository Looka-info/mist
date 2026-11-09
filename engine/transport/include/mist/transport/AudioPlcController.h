#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <atomic>

namespace mist::transport {

using AudioPacketLostCallback = std::function<void(uint16_t lostCount, uint32_t estimatedSamples)>;

class AudioPlcController {
public:
    explicit AudioPlcController(uint32_t samplesPerPacket = 960) noexcept;
    ~AudioPlcController() = default;

    void setPacketLostCallback(AudioPacketLostCallback cb) { lostCb_ = std::move(cb); }

    // Inspects every incoming audio packet. If gaps are detected, triggers PLC callback.
    void onAudioPacketReceived(uint16_t seqNumber, uint32_t rtpTimestamp) noexcept;

    [[nodiscard]] uint64_t getPacketsReceived() const noexcept { return packetsReceived_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getPacketsLost() const noexcept { return packetsLost_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getPlcConcealmentsTriggered() const noexcept { return plcConcealments_.load(std::memory_order_relaxed); }

    void reset() noexcept;

private:
    mutable std::mutex mutex_;
    uint32_t samplesPerPacket_{960}; // 20ms @ 48kHz = 960 samples
    AudioPacketLostCallback lostCb_;

    bool hasReceivedFirst_{false};
    uint16_t expectedSeq_{0};
    uint32_t lastTimestamp_{0};

    std::atomic<uint64_t> packetsReceived_{0};
    std::atomic<uint64_t> packetsLost_{0};
    std::atomic<uint64_t> plcConcealments_{0};
};

} // namespace mist::transport
