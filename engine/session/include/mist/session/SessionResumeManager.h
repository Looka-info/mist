#pragma once

#include "mist/common/Result.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdint>

namespace mist::session {

struct SessionResumeToken {
    std::string rawToken;
    std::string sessionId;
    std::string guestId;
    uint64_t issuedAtSec{0};
    uint64_t expiresAtSec{0};
    uint64_t nonce{0};
    std::string signatureHex;

    [[nodiscard]] bool isExpired(uint64_t currentEpochSec) const noexcept {
        return currentEpochSec > expiresAtSec;
    }
};

struct SessionResumeConfig {
    uint32_t tokenValiditySec{180}; // 3 minutes validity window
    bool enableRollingTokens{true};  // Issue fresh token on each successful resume
};

class SessionResumeManager {
public:
    explicit SessionResumeManager(std::string sessionSecret, SessionResumeConfig config = {}) noexcept;
    ~SessionResumeManager() = default;

    // Host generates a signed resume token for an active guest session
    [[nodiscard]] common::Result<SessionResumeToken> issueToken(
        const std::string& sessionId,
        const std::string& guestId,
        uint64_t currentEpochSec
    );

    // Host validates an incoming resume request from a reconnecting guest
    // Verifies cryptographic signature, expiration, and non-replay, then consumes the token
    [[nodiscard]] common::Result<std::string> validateAndConsumeToken(
        const std::string& rawToken,
        const std::string& guestId,
        uint64_t currentEpochSec
    );

    // Invalidate all tokens for a guest (e.g. guest kicked or disconnected permanently)
    void invalidateGuestTokens(const std::string& guestId);

    // Invalidate all active tokens for this session
    void invalidateAll();

    // Helper: compute HMAC-SHA256 signature hex
    [[nodiscard]] static std::string signPayload(std::string_view payload, std::string_view secret) noexcept;

    // Helper: parse raw token string into component struct
    [[nodiscard]] static common::Result<SessionResumeToken> parseToken(const std::string& rawToken) noexcept;

private:
    mutable std::mutex mutex_;
    std::string sessionSecret_;
    SessionResumeConfig config_;
    uint64_t nonceCounter_{1000};

    // consumed tokens to prevent replay attacks
    std::unordered_set<std::string> consumedTokens_;
    // active token by guestId
    std::unordered_map<std::string, SessionResumeToken> activeTokens_;
};

} // namespace mist::session
