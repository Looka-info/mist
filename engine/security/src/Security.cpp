#include "mist/security/Security.h"

namespace mist::security {

common::Result<void> DeviceIdentity::validate() const noexcept {
    if (deviceId.empty() || deviceId.size() < 8) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InvalidArgument, "Device ID must be at least 8 characters");
    }
    if (deviceName.empty()) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InvalidArgument, "Device name cannot be empty");
    }
    return common::Result<void>::success();
}

bool AuthToken::isExpired(uint64_t currentEpochSec) const noexcept {
    return expiresAtEpochSec > 0 && currentEpochSec >= expiresAtEpochSec;
}

common::Result<void> AuthToken::validate() const noexcept {
    if (rawToken.empty() || rawToken.size() < 16) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InvalidArgument, "Auth token must be at least 16 characters");
    }
    return common::Result<void>::success();
}

common::Result<void> SessionCredentials::validate() const noexcept {
    if (sessionId.empty()) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InvalidArgument, "Session ID cannot be empty");
    }
    if (sharedSecretHex.empty() || sharedSecretHex.size() < 32) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InvalidArgument, "Shared secret must be at least 32 hex characters");
    }
    return clientToken.validate();
}

bool constantTimeEquals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }

    // volatile accumulator to ensure compiler does not optimize early exit
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

} // namespace mist::security
