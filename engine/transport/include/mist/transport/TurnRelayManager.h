#pragma once

#include "mist/common/Result.h"
#include "mist/transport/WebRtcConfig.h"
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>
#include <mutex>

namespace mist::transport {

enum class TurnProtocol {
    Udp,
    Tcp,
    Tls
};

enum class TurnCredentialType {
    Static,
    EphemeralRest
};

enum class RelayState {
    Unconfigured,
    Probing,
    Available,
    ActiveInUse,
    FallbackEngaged,
    Unreachable,
    Failed
};

struct TurnServerEndpoint {
    std::string host;
    uint16_t port{3478};
    TurnProtocol protocol{TurnProtocol::Udp};
    std::string rawUri;
    std::string username;
    std::string credential;
    uint64_t expiresAtEpochSec{0};

    [[nodiscard]] bool isExpired(uint64_t nowEpochSec) const noexcept {
        return expiresAtEpochSec > 0 && nowEpochSec >= expiresAtEpochSec;
    }
};

struct RelayDiagnostics {
    RelayState state{RelayState::Unconfigured};
    std::string activeRelayUri;
    TurnProtocol activeProtocol{TurnProtocol::Udp};
    double relayRttMs{0.0};
    double directRttMs{0.0};
    double latencyPenaltyMs{0.0};
    bool isRelayed{false};
    uint64_t bytesRelayed{0};
    std::string failureReason;
};

class TurnCredentialGenerator {
public:
    [[nodiscard]] static std::pair<std::string, std::string> generateRestCredentials(
        const std::string& sharedSecret,
        const std::string& peerIdentity,
        uint32_t ttlSeconds,
        uint64_t nowEpochSec
    );

    [[nodiscard]] static bool verifyRestCredentials(
        const std::string& sharedSecret,
        const std::string& username,
        const std::string& credential,
        uint64_t nowEpochSec
    );

    [[nodiscard]] static std::optional<TurnServerEndpoint> parseTurnUri(
        const std::string& uri,
        const std::string& username = "",
        const std::string& credential = "",
        uint64_t expiresAt = 0
    ) noexcept;
};

class TurnRelayManager {
public:
    TurnRelayManager() noexcept = default;
    ~TurnRelayManager() = default;

    [[nodiscard]] common::Result<void> configureRelays(const std::vector<TurnServerEndpoint>& endpoints);
    [[nodiscard]] std::vector<IceServer> getActiveIceServers(uint64_t nowEpochSec) const;
    [[nodiscard]] std::vector<TurnServerEndpoint> getEndpoints() const;

    void updateRelayState(RelayState newState, const std::string& uri = "", TurnProtocol proto = TurnProtocol::Udp);
    void recordRelayMetrics(double relayRttMs, double directRttMs, uint64_t bytesSentOrReceived);
    void recordRelayFailure(const std::string& reason);

    [[nodiscard]] RelayDiagnostics getDiagnostics() const noexcept;
    [[nodiscard]] bool isRelayAvailable() const noexcept;
    [[nodiscard]] bool isRelayInUse() const noexcept;

    void reset() noexcept;

private:
    mutable std::mutex mutex_;
    std::vector<TurnServerEndpoint> endpoints_;
    RelayDiagnostics diagnostics_;
};

} // namespace mist::transport
