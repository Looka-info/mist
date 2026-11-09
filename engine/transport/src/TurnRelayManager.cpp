#include "mist/transport/TurnRelayManager.h"
#include "mist/common/Logger.h"
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include <sstream>
#include <algorithm>

namespace mist::transport {

std::pair<std::string, std::string> TurnCredentialGenerator::generateRestCredentials(
    const std::string& sharedSecret,
    const std::string& peerIdentity,
    uint32_t ttlSeconds,
    uint64_t nowEpochSec
) {
    uint64_t expiry = nowEpochSec + ttlSeconds;
    std::string username = std::to_string(expiry) + ":" + peerIdentity;

    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (!mdInfo) {
        return {username, ""};
    }

    unsigned char hmacResult[20] = {0};
    int ret = mbedtls_md_hmac(
        mdInfo,
        reinterpret_cast<const unsigned char*>(sharedSecret.data()),
        sharedSecret.size(),
        reinterpret_cast<const unsigned char*>(username.data()),
        username.size(),
        hmacResult
    );

    if (ret != 0) {
        return {username, ""};
    }

    unsigned char base64Buf[64] = {0};
    size_t outLen = 0;
    ret = mbedtls_base64_encode(base64Buf, sizeof(base64Buf), &outLen, hmacResult, sizeof(hmacResult));
    if (ret != 0) {
        return {username, ""};
    }

    std::string credential(reinterpret_cast<char*>(base64Buf), outLen);
    return {username, credential};
}

bool TurnCredentialGenerator::verifyRestCredentials(
    const std::string& sharedSecret,
    const std::string& username,
    const std::string& credential,
    uint64_t nowEpochSec
) {
    auto colonPos = username.find(':');
    if (colonPos == std::string::npos) return false;

    try {
        uint64_t expiry = std::stoull(username.substr(0, colonPos));
        if (nowEpochSec >= expiry) {
            return false; // Credential expired
        }
    } catch (...) {
        return false;
    }

    const mbedtls_md_info_t* mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (!mdInfo) return false;

    unsigned char hmacResult[20] = {0};
    if (mbedtls_md_hmac(
            mdInfo,
            reinterpret_cast<const unsigned char*>(sharedSecret.data()),
            sharedSecret.size(),
            reinterpret_cast<const unsigned char*>(username.data()),
            username.size(),
            hmacResult) != 0) {
        return false;
    }

    unsigned char base64Buf[64] = {0};
    size_t outLen = 0;
    if (mbedtls_base64_encode(base64Buf, sizeof(base64Buf), &outLen, hmacResult, sizeof(hmacResult)) != 0) {
        return false;
    }

    std::string expectedCred(reinterpret_cast<char*>(base64Buf), outLen);
    return (expectedCred == credential);
}

std::optional<TurnServerEndpoint> TurnCredentialGenerator::parseTurnUri(
    const std::string& uri,
    const std::string& username,
    const std::string& credential,
    uint64_t expiresAt
) noexcept {
    TurnServerEndpoint ep{};
    ep.rawUri = uri;
    ep.username = username;
    ep.credential = credential;
    ep.expiresAtEpochSec = expiresAt;

    std::string remainder = uri;
    if (remainder.starts_with("turns:")) {
        ep.protocol = TurnProtocol::Tls;
        ep.port = 5349;
        remainder = remainder.substr(6);
    } else if (remainder.starts_with("turn:")) {
        ep.protocol = TurnProtocol::Udp;
        ep.port = 3478;
        remainder = remainder.substr(5);
    } else {
        return std::nullopt;
    }

    auto qPos = remainder.find('?');
    if (qPos != std::string::npos) {
        std::string query = remainder.substr(qPos + 1);
        remainder = remainder.substr(0, qPos);
        if (query.find("transport=tcp") != std::string::npos) {
            if (ep.protocol != TurnProtocol::Tls) {
                ep.protocol = TurnProtocol::Tcp;
            }
        }
    }

    auto colonPos = remainder.find(':');
    if (colonPos != std::string::npos) {
        ep.host = remainder.substr(0, colonPos);
        try {
            ep.port = static_cast<uint16_t>(std::stoul(remainder.substr(colonPos + 1)));
        } catch (...) {}
    } else {
        ep.host = remainder;
    }

    if (ep.host.empty()) return std::nullopt;
    return ep;
}

// -----------------------------------------------------------------------------
// TurnRelayManager implementation
// -----------------------------------------------------------------------------

common::Result<void> TurnRelayManager::configureRelays(const std::vector<TurnServerEndpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_ = endpoints;
    if (endpoints_.empty()) {
        diagnostics_.state = RelayState::Unconfigured;
    } else {
        diagnostics_.state = RelayState::Available;
        MIST_LOG_INFO(common::SubsystemId::Transport, "Configured " + std::to_string(endpoints_.size()) + " TURN relay endpoint(s).");
    }
    return common::Result<void>::success();
}

std::vector<IceServer> TurnRelayManager::getActiveIceServers(uint64_t nowEpochSec) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<IceServer> out;
    for (const auto& ep : endpoints_) {
        if (!ep.isExpired(nowEpochSec)) {
            IceServer s{};
            s.url = ep.rawUri;
            s.username = ep.username;
            s.credential = ep.credential;
            out.push_back(s);
        }
    }
    return out;
}

std::vector<TurnServerEndpoint> TurnRelayManager::getEndpoints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return endpoints_;
}

void TurnRelayManager::updateRelayState(RelayState newState, const std::string& uri, TurnProtocol proto) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.state = newState;
    if (!uri.empty()) diagnostics_.activeRelayUri = uri;
    diagnostics_.activeProtocol = proto;
    diagnostics_.isRelayed = (newState == RelayState::ActiveInUse || newState == RelayState::FallbackEngaged);

    MIST_LOG_INFO(common::SubsystemId::Transport, "Relay state updated to: " + std::to_string(static_cast<int>(newState)) +
                  (diagnostics_.isRelayed ? " [IN USE]" : ""));
}

void TurnRelayManager::recordRelayMetrics(double relayRttMs, double directRttMs, uint64_t bytesRelayed) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.relayRttMs = relayRttMs;
    diagnostics_.directRttMs = directRttMs;
    if (directRttMs > 0.0 && relayRttMs > directRttMs) {
        diagnostics_.latencyPenaltyMs = relayRttMs - directRttMs;
    } else {
        diagnostics_.latencyPenaltyMs = 0.0;
    }
    diagnostics_.bytesRelayed += bytesRelayed;
}

void TurnRelayManager::recordRelayFailure(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.state = RelayState::Failed;
    diagnostics_.failureReason = reason;
    diagnostics_.isRelayed = false;
    MIST_LOG_ERROR(common::SubsystemId::Transport, "TURN relay failure: " + reason);
}

RelayDiagnostics TurnRelayManager::getDiagnostics() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return diagnostics_;
}

bool TurnRelayManager::isRelayAvailable() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return diagnostics_.state == RelayState::Available ||
           diagnostics_.state == RelayState::ActiveInUse ||
           diagnostics_.state == RelayState::FallbackEngaged;
}

bool TurnRelayManager::isRelayInUse() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return diagnostics_.isRelayed;
}

void TurnRelayManager::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoints_.clear();
    diagnostics_ = RelayDiagnostics{};
}

} // namespace mist::transport
