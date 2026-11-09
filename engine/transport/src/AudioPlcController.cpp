#include "mist/transport/AudioPlcController.h"

namespace mist::transport {

AudioPlcController::AudioPlcController(uint32_t samplesPerPacket) noexcept
    : samplesPerPacket_(samplesPerPacket > 0 ? samplesPerPacket : 960) {}

void AudioPlcController::onAudioPacketReceived(uint16_t seqNumber, uint32_t rtpTimestamp) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    packetsReceived_.fetch_add(1, std::memory_order_relaxed);

    if (!hasReceivedFirst_) {
        hasReceivedFirst_ = true;
        expectedSeq_ = static_cast<uint16_t>(seqNumber + 1);
        lastTimestamp_ = rtpTimestamp;
        return;
    }

    int16_t diff = static_cast<int16_t>(seqNumber - expectedSeq_);
    if (diff > 0) {
        // Gap in audio packets! Trigger Packet Loss Concealment (PLC)
        uint16_t lostCount = static_cast<uint16_t>(diff);
        packetsLost_.fetch_add(lostCount, std::memory_order_relaxed);
        plcConcealments_.fetch_add(lostCount, std::memory_order_relaxed);

        if (lostCb_) {
            uint32_t estimatedSamples = static_cast<uint32_t>(lostCount) * samplesPerPacket_;
            lostCb_(lostCount, estimatedSamples);
        }

        expectedSeq_ = static_cast<uint16_t>(seqNumber + 1);
    } else if (diff == 0) {
        // Exactly next packet
        expectedSeq_ = static_cast<uint16_t>(seqNumber + 1);
    }
    // If diff < 0, it's a late or duplicate packet; expectedSeq_ remains unchanged

    lastTimestamp_ = rtpTimestamp;
}

void AudioPlcController::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    hasReceivedFirst_ = false;
    expectedSeq_ = 0;
    lastTimestamp_ = 0;
}

} // namespace mist::transport
