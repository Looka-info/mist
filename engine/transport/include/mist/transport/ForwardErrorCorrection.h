#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <utility>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <string>

namespace mist::transport {

// RFC 5109 / RFC 8627-compliant FEC packet structure
struct FecPacket {
    uint16_t baseSequenceNumber{0};
    uint16_t mask{0};           // Bit 0 corresponds to base, bit 1 to base+1, etc.
    uint16_t lengthRecovery{0}; // XOR sum of protected payload lengths
    std::vector<uint8_t> payload;
};

class FecEncoder {
public:
    explicit FecEncoder(uint32_t stepSize = 5) noexcept;
    ~FecEncoder() = default;

    // Set how many media packets trigger 1 FEC parity packet (default 5 = 20% protection)
    void setStepSize(uint32_t stepSize) noexcept;
    [[nodiscard]] uint32_t getStepSize() const noexcept { return stepSize_; }

    // Adds a media packet to current FEC group. If group size reached, generates an FEC packet.
    void addMediaPacket(uint16_t seqNumber, std::span<const uint8_t> payload);

    // Forces generation of an FEC packet for any pending partial group (e.g. at frame boundary)
    [[nodiscard]] std::vector<FecPacket> generatePendingFecPackets();

    void reset() noexcept;

private:
    uint32_t stepSize_{5};
    struct MediaEntry {
        uint16_t seq;
        std::vector<uint8_t> data;
    };
    std::vector<MediaEntry> currentGroup_;
};

class FecDecoder {
public:
    FecDecoder() noexcept = default;
    ~FecDecoder() = default;

    void addMediaPacket(uint16_t seqNumber, std::span<const uint8_t> payload);
    void addFecPacket(const FecPacket& fec);

    // Attempts to recover any lost media packets from stored FEC and media packets.
    // Returns list of pairs: (recoveredSequenceNumber, recoveredPayload)
    [[nodiscard]] std::vector<std::pair<uint16_t, std::vector<uint8_t>>> tryRecoverPackets();

    [[nodiscard]] uint64_t getPacketsRecovered() const noexcept { return packetsRecovered_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getFecPacketsReceived() const noexcept { return fecPacketsReceived_.load(std::memory_order_relaxed); }

    void purgeOldPackets(uint16_t currentSeqNumber, uint16_t maxAge = 64) noexcept;
    void reset() noexcept;

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint16_t, std::vector<uint8_t>> receivedMedia_;
    std::vector<FecPacket> pendingFec_;
    std::atomic<uint64_t> packetsRecovered_{0};
    std::atomic<uint64_t> fecPacketsReceived_{0};
};

enum class RecoveryReason : uint8_t {
    UnrecoverableLoss,
    DecoderDesync,
    NewGuestJoined,
    PeriodicRefresh
};

struct RapidRecoveryEvent {
    std::string guestId;
    RecoveryReason reason;
    uint64_t timestampNs{0};
    bool isKeyframeTriggered{false};
    std::string details;
};

class RapidReferenceRecovery {
public:
    explicit RapidReferenceRecovery(uint32_t debounceIntervalMs = 200) noexcept;
    ~RapidReferenceRecovery() = default;

    void setDebounceInterval(uint32_t debounceIntervalMs) noexcept { debounceIntervalMs_ = debounceIntervalMs; }

    // Evaluates whether an immediate keyframe refresh should be triggered
    [[nodiscard]] RapidRecoveryEvent requestRecovery(
        const std::string& guestId,
        RecoveryReason reason,
        uint64_t nowNs,
        const std::string& details = ""
    ) noexcept;

    [[nodiscard]] uint64_t getTotalRecoveryRequests() const noexcept { return totalRequests_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getKeyframesDispatched() const noexcept { return keyframesDispatched_.load(std::memory_order_relaxed); }

    void reset() noexcept;

private:
    mutable std::mutex mutex_;
    uint32_t debounceIntervalMs_{200};
    uint64_t lastKeyframeTimeNs_{0};

    std::atomic<uint64_t> totalRequests_{0};
    std::atomic<uint64_t> keyframesDispatched_{0};
};

} // namespace mist::transport
