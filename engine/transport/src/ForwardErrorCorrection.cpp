#include "mist/transport/ForwardErrorCorrection.h"
#include <algorithm>

namespace mist::transport {

FecEncoder::FecEncoder(uint32_t stepSize) noexcept
    : stepSize_(std::max(1u, stepSize)) {}

void FecEncoder::setStepSize(uint32_t stepSize) noexcept {
    stepSize_ = std::max(1u, stepSize);
}

void FecEncoder::addMediaPacket(uint16_t seqNumber, std::span<const uint8_t> payload) {
    MediaEntry entry{
        .seq = seqNumber,
        .data = std::vector<uint8_t>(payload.begin(), payload.end())
    };
    currentGroup_.push_back(std::move(entry));
}

std::vector<FecPacket> FecEncoder::generatePendingFecPackets() {
    std::vector<FecPacket> result;
    if (currentGroup_.empty()) {
        return result;
    }

    FecPacket fec{};
    fec.baseSequenceNumber = currentGroup_[0].seq;

    size_t maxLen = 0;
    for (const auto& entry : currentGroup_) {
        maxLen = std::max(maxLen, entry.data.size());
        int16_t diff = static_cast<int16_t>(entry.seq - fec.baseSequenceNumber);
        if (diff >= 0 && diff < 16) {
            fec.mask |= static_cast<uint16_t>(1u << static_cast<uint16_t>(diff));
        }
        fec.lengthRecovery ^= static_cast<uint16_t>(entry.data.size());
    }

    fec.payload.resize(maxLen, 0);
    for (const auto& entry : currentGroup_) {
        for (size_t b = 0; b < entry.data.size(); ++b) {
            fec.payload[b] ^= entry.data[b];
        }
    }

    result.push_back(std::move(fec));
    currentGroup_.clear();
    return result;
}

void FecEncoder::reset() noexcept {
    currentGroup_.clear();
}

void FecDecoder::addMediaPacket(uint16_t seqNumber, std::span<const uint8_t> payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    receivedMedia_[seqNumber] = std::vector<uint8_t>(payload.begin(), payload.end());
}

void FecDecoder::addFecPacket(const FecPacket& fec) {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingFec_.push_back(fec);
    fecPacketsReceived_.fetch_add(1, std::memory_order_relaxed);
}

std::vector<std::pair<uint16_t, std::vector<uint8_t>>> FecDecoder::tryRecoverPackets() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<uint16_t, std::vector<uint8_t>>> recovered;

    bool progress = true;
    while (progress) {
        progress = false;

        for (auto it = pendingFec_.begin(); it != pendingFec_.end();) {
            std::vector<uint16_t> missingSeqs;
            std::vector<uint16_t> presentSeqs;

            for (uint16_t bit = 0; bit < 16; ++bit) {
                if ((it->mask & (1u << bit)) != 0) {
                    uint16_t seq = static_cast<uint16_t>(it->baseSequenceNumber + bit);
                    if (receivedMedia_.find(seq) != receivedMedia_.end()) {
                        presentSeqs.push_back(seq);
                    } else {
                        missingSeqs.push_back(seq);
                    }
                }
            }

            if (missingSeqs.size() == 1) {
                // Exactly one packet missing: perform XOR recovery
                uint16_t targetSeq = missingSeqs[0];

                // Calculate exact recovered payload length
                uint16_t recoveredLen = it->lengthRecovery;
                for (uint16_t s : presentSeqs) {
                    recoveredLen ^= static_cast<uint16_t>(receivedMedia_[s].size());
                }

                std::vector<uint8_t> recoveredData = it->payload;
                for (uint16_t s : presentSeqs) {
                    const auto& pkt = receivedMedia_[s];
                    for (size_t b = 0; b < pkt.size() && b < recoveredData.size(); ++b) {
                        recoveredData[b] ^= pkt[b];
                    }
                }

                if (recoveredData.size() > recoveredLen) {
                    recoveredData.resize(recoveredLen);
                }

                receivedMedia_[targetSeq] = recoveredData;
                recovered.push_back({targetSeq, recoveredData});
                packetsRecovered_.fetch_add(1, std::memory_order_relaxed);

                it = pendingFec_.erase(it);
                progress = true;
                continue;
            } else if (missingSeqs.empty()) {
                // All packets already received, FEC no longer needed
                it = pendingFec_.erase(it);
                continue;
            }

            ++it;
        }
    }

    return recovered;
}

void FecDecoder::purgeOldPackets(uint16_t currentSeqNumber, uint16_t maxAge) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = receivedMedia_.begin(); it != receivedMedia_.end();) {
        int16_t diff = static_cast<int16_t>(currentSeqNumber - it->first);
        if (diff > static_cast<int16_t>(maxAge)) {
            it = receivedMedia_.erase(it);
        } else {
            ++it;
        }
    }
}

void FecDecoder::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    receivedMedia_.clear();
    pendingFec_.clear();
}

RapidReferenceRecovery::RapidReferenceRecovery(uint32_t debounceIntervalMs) noexcept
    : debounceIntervalMs_(debounceIntervalMs) {}

RapidRecoveryEvent RapidReferenceRecovery::requestRecovery(
    const std::string& guestId,
    RecoveryReason reason,
    uint64_t nowNs,
    const std::string& details
) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    totalRequests_.fetch_add(1, std::memory_order_relaxed);

    RapidRecoveryEvent ev{
        .guestId = guestId,
        .reason = reason,
        .timestampNs = nowNs,
        .isKeyframeTriggered = false,
        .details = details
    };

    uint64_t debounceNs = static_cast<uint64_t>(debounceIntervalMs_) * 1000000ULL;
    if (lastKeyframeTimeNs_ == 0 || (nowNs - lastKeyframeTimeNs_) >= debounceNs || reason == RecoveryReason::NewGuestJoined) {
        lastKeyframeTimeNs_ = nowNs;
        keyframesDispatched_.fetch_add(1, std::memory_order_relaxed);
        ev.isKeyframeTriggered = true;
    }

    return ev;
}

void RapidReferenceRecovery::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    lastKeyframeTimeNs_ = 0;
}

} // namespace mist::transport
