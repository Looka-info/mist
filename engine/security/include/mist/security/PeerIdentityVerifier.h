#pragma once

#include "mist/common/Result.h"
#include <string>
#include <string_view>
#include <mutex>
#include <cstdint>

namespace mist::security {

enum class IdentityTrustState : uint8_t {
    Unverified = 0,
    Verified = 1,
    FingerprintMismatch = 2,
    IdentityChanged = 3,
    NoFingerprintInSdp = 4
};

[[nodiscard]] constexpr std::string_view identityTrustStateToString(IdentityTrustState s) noexcept {
    switch (s) {
        case IdentityTrustState::Unverified:          return "Unverified";
        case IdentityTrustState::Verified:            return "Verified";
        case IdentityTrustState::FingerprintMismatch: return "FingerprintMismatch";
        case IdentityTrustState::IdentityChanged:     return "IdentityChanged";
        case IdentityTrustState::NoFingerprintInSdp:  return "NoFingerprintInSdp";
    }
    return "Unknown";
}

struct SecurityDiagnostics {
    bool isEncrypted{true};
    std::string cipherSuite{"TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256"};
    std::string localFingerprint;
    std::string remoteFingerprint;
    std::string expectedFingerprint;
    std::string shortAuthString;
    IdentityTrustState trustState{IdentityTrustState::Unverified};
    std::string statusMessage{"Unverified"};
};

class PeerIdentityVerifier {
public:
    PeerIdentityVerifier() noexcept = default;
    ~PeerIdentityVerifier() = default;

    // Set expected remote peer certificate fingerprint (e.g. established during authenticated pairing)
    void setExpectedRemoteFingerprint(std::string expectedFp);

    // Set local certificate fingerprint
    void setLocalFingerprint(std::string localFp);

    // Set session context for SAS computation
    void setSessionContext(std::string sessionId, std::string authToken);

    // Verifies remote description SDP against expected peer identity and checks for in-flight identity change
    [[nodiscard]] common::Result<void> verifyRemoteDescription(const std::string& sdp);

    [[nodiscard]] SecurityDiagnostics getDiagnostics() const;
    [[nodiscard]] IdentityTrustState getTrustState() const;
    [[nodiscard]] std::string getShortAuthString() const;

    void reset() noexcept;

    // Static helper utilities
    [[nodiscard]] static std::string extractFingerprintFromSdp(std::string_view sdp);
    [[nodiscard]] static std::string normalizeFingerprint(std::string_view fp);
    [[nodiscard]] static std::string calculateSas(
        std::string_view hostFp,
        std::string_view guestFp,
        std::string_view sessionId,
        std::string_view authToken
    );

private:
    void updateSasUnderLock();

    mutable std::mutex mutex_;
    std::string expectedRemoteFingerprint_;
    std::string localFingerprint_;
    std::string activeRemoteFingerprint_;
    std::string sessionId_;
    std::string authToken_;
    std::string shortAuthString_;
    IdentityTrustState trustState_{IdentityTrustState::Unverified};
    std::string statusMessage_{"Unverified"};
};

} // namespace mist::security
