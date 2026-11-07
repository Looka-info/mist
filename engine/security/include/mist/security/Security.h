#pragma once

#include "mist/common/Result.h"
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace mist::security {

struct DeviceIdentity {
    std::string deviceId;
    std::string deviceName;
    std::string publicKeyFingerprint;

    [[nodiscard]] common::Result<void> validate() const noexcept;
};

struct AuthToken {
    std::string rawToken;
    uint64_t expiresAtEpochSec{0};

    [[nodiscard]] bool isExpired(uint64_t currentEpochSec) const noexcept;
    [[nodiscard]] common::Result<void> validate() const noexcept;
};

struct SessionCredentials {
    std::string sessionId;
    AuthToken clientToken;
    std::string sharedSecretHex;

    [[nodiscard]] common::Result<void> validate() const noexcept;
};

// Constant-time memory comparison to protect against side-channel timing attacks
[[nodiscard]] bool constantTimeEquals(std::string_view a, std::string_view b) noexcept;

class ISecurityProvider {
public:
    virtual ~ISecurityProvider() = default;

    [[nodiscard]] virtual common::Result<DeviceIdentity> getLocalIdentity() = 0;
    [[nodiscard]] virtual common::Result<AuthToken> generatePairingToken(uint32_t lifetimeSec) = 0;
    [[nodiscard]] virtual common::Result<void> verifyToken(const AuthToken& token) = 0;
    [[nodiscard]] virtual common::Result<SessionCredentials> establishSessionCredentials(const std::string& peerDeviceId) = 0;
};

} // namespace mist::security
