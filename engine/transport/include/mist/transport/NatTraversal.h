#pragma once

#include "mist/common/Result.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <memory>
#include <chrono>
#include <mutex>

namespace mist::transport {

enum class CandidateType {
    Host,
    ServerReflexive,
    PeerReflexive,
    Relayed,
    Unknown
};

enum class AddressFamily {
    IPv4,
    IPv6,
    Unknown
};

enum class NatType {
    Unknown,
    OpenInternetDirect,
    FullCone,
    AddressRestrictedCone,
    PortRestrictedCone,
    Symmetric,
    CarrierGradeNat
};

enum class ConnectionFailureReason {
    None,
    SignalingUnreachable,
    SignalingTimeout,
    IceGatheringFailed,
    IceConnectivityTimeout,
    SymmetricNatMismatch,
    FirewallBlocked,
    NetworkChanged,
    PeerDisconnected
};

struct ParsedIceCandidate {
    std::string rawCandidate;
    std::string foundation;
    uint32_t component{1};
    std::string transport{"udp"};
    uint32_t priority{0};
    std::string ip;
    uint16_t port{0};
    CandidateType type{CandidateType::Unknown};
    std::string relatedIp;
    uint16_t relatedPort{0};
    AddressFamily family{AddressFamily::Unknown};
    bool isCgnat{false};
    bool isPrivateLan{false};
    bool isLoopback{false};
};

struct NatDiagnostics {
    NatType detectedNatType{NatType::Unknown};
    AddressFamily localFamily{AddressFamily::IPv4};
    uint32_t hostCandidatesCount{0};
    uint32_t srflxCandidatesCount{0};
    uint32_t relayCandidatesCount{0};
    uint32_t ipv4CandidatesCount{0};
    uint32_t ipv6CandidatesCount{0};
    bool hasDirectRoute{false};
    bool isBehindCgnat{false};
    bool isBehindSymmetricNat{false};
    std::string activeLocalCandidateType{"none"};
    std::string activeRemoteCandidateType{"none"};
    std::string selectedPairSummary{"none"};
    ConnectionFailureReason failureReason{ConnectionFailureReason::None};
};

struct PortMappingLease {
    uint16_t internalPort{0};
    uint16_t externalPort{0};
    std::string protocol{"UDP"};
    uint32_t leaseDurationSeconds{3600};
    std::chrono::steady_clock::time_point leaseExpiresAt;
    std::string description;
    bool isActive{false};
};

class IceCandidatePrioritizer {
public:
    static uint32_t computeRfc8445Priority(CandidateType type, AddressFamily family, uint32_t component = 1) noexcept;
    static std::optional<ParsedIceCandidate> parseSdpCandidate(const std::string& candidateLine) noexcept;
    static bool isCgnatAddress(const std::string& ip) noexcept;
    static bool isPrivateAddress(const std::string& ip) noexcept;
    static bool isLoopbackAddress(const std::string& ip) noexcept;
    static AddressFamily determineAddressFamily(const std::string& ip) noexcept;
    static const char* candidateTypeToString(CandidateType type) noexcept;
    static const char* natTypeToString(NatType type) noexcept;
    static const char* failureReasonToString(ConnectionFailureReason reason) noexcept;
    static NatType classifyNatBehavior(const std::vector<ParsedIceCandidate>& localCandidates) noexcept;
    static ConnectionFailureReason classifyFailure(
        bool signalingConnected,
        bool candidatesGathered,
        bool iceConnected,
        bool isSymmetricBothSides
    ) noexcept;
};

class PortMappingManager {
public:
    PortMappingManager() noexcept = default;
    ~PortMappingManager() { releaseAllMappings(); }

    [[nodiscard]] common::Result<PortMappingLease> requestPortMapping(
        uint16_t internalPort,
        uint16_t requestedExternalPort,
        const std::string& protocol = "UDP",
        uint32_t leaseSeconds = 3600,
        const std::string& description = "Mist Streaming Media"
    );

    [[nodiscard]] common::Result<void> releasePortMapping(uint16_t externalPort, const std::string& protocol = "UDP");
    void releaseAllMappings();

    [[nodiscard]] std::vector<PortMappingLease> getActiveLeases() const;
    [[nodiscard]] bool hasActiveLease(uint16_t externalPort) const noexcept;

private:
    mutable std::mutex mutex_;
    std::vector<PortMappingLease> activeLeases_;
};

class IceRestartController {
public:
    explicit IceRestartController(uint32_t maxRestarts = 3) noexcept;

    bool canAttemptRestart() const noexcept;
    void recordRestartAttempt() noexcept;
    void reset() noexcept;
    [[nodiscard]] uint32_t getAttemptCount() const noexcept { return restartCount_; }
    [[nodiscard]] uint32_t getGeneration() const noexcept { return iceGeneration_; }

private:
    uint32_t maxRestarts_{3};
    uint32_t restartCount_{0};
    uint32_t iceGeneration_{0};
    std::chrono::steady_clock::time_point lastRestartTime_{};
};

} // namespace mist::transport
