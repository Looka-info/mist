#include "mist/session/SessionResumeManager.h"
#include <sstream>
#include <vector>
#include <array>
#include <cstring>

namespace mist::session {

namespace {

inline uint32_t rotr(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) noexcept {
    return (e & f) ^ (~e & g);
}

inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) noexcept {
    return (a & b) ^ (a & c) ^ (b & c);
}

inline uint32_t sig0(uint32_t x) noexcept {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline uint32_t sig1(uint32_t x) noexcept {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline uint32_t theta0(uint32_t x) noexcept {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline uint32_t theta1(uint32_t x) noexcept {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

std::array<uint8_t, 32> computeSha256(const uint8_t* data, size_t len) noexcept {
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint64_t bitLen = static_cast<uint64_t>(len) * 8ULL;
    size_t paddedLen = ((len + 8) / 64 + 1) * 64;
    std::vector<uint8_t> buffer(paddedLen, 0);
    if (len > 0) {
        std::memcpy(buffer.data(), data, len);
    }
    buffer[len] = 0x80;

    for (int i = 0; i < 8; ++i) {
        buffer[paddedLen - 1 - static_cast<size_t>(i)] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF);
    }

    for (size_t chunk = 0; chunk < paddedLen; chunk += 64) {
        uint32_t W[64];
        for (int t = 0; t < 16; ++t) {
            size_t idx = chunk + static_cast<size_t>(t * 4);
            W[t] = (static_cast<uint32_t>(buffer[idx]) << 24) |
                   (static_cast<uint32_t>(buffer[idx + 1]) << 16) |
                   (static_cast<uint32_t>(buffer[idx + 2]) << 8) |
                   (static_cast<uint32_t>(buffer[idx + 3]));
        }
        for (int t = 16; t < 64; ++t) {
            W[t] = theta1(W[t - 2]) + W[t - 7] + theta0(W[t - 15]) + W[t - 16];
        }

        uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
        uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

        for (int t = 0; t < 64; ++t) {
            uint32_t T1 = h + sig1(e) + choose(e, f, g) + K[t] + W[t];
            uint32_t T2 = sig0(a) + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }

        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    std::array<uint8_t, 32> digest{};
    for (int i = 0; i < 8; ++i) {
        size_t idx = static_cast<size_t>(i * 4);
        digest[idx]     = static_cast<uint8_t>((H[i] >> 24) & 0xFF);
        digest[idx + 1] = static_cast<uint8_t>((H[i] >> 16) & 0xFF);
        digest[idx + 2] = static_cast<uint8_t>((H[i] >> 8) & 0xFF);
        digest[idx + 3] = static_cast<uint8_t>(H[i] & 0xFF);
    }
    return digest;
}

std::array<uint8_t, 32> computeHmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* msg, size_t msgLen) {
    uint8_t k0[64] = {0};
    if (keyLen > 64) {
        auto h = computeSha256(key, keyLen);
        std::memcpy(k0, h.data(), 32);
    } else if (keyLen > 0) {
        std::memcpy(k0, key, keyLen);
    }

    uint8_t ipad[64];
    uint8_t opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = static_cast<uint8_t>(k0[i] ^ 0x36);
        opad[i] = static_cast<uint8_t>(k0[i] ^ 0x5C);
    }

    std::vector<uint8_t> inner(64 + msgLen);
    std::memcpy(inner.data(), ipad, 64);
    if (msgLen > 0) {
        std::memcpy(inner.data() + 64, msg, msgLen);
    }
    auto innerHash = computeSha256(inner.data(), inner.size());

    std::vector<uint8_t> outer(64 + 32);
    std::memcpy(outer.data(), opad, 64);
    std::memcpy(outer.data() + 64, innerHash.data(), 32);
    return computeSha256(outer.data(), outer.size());
}

std::string toHex(const std::array<uint8_t, 32>& bytes) {
    static const char hexChars[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(64);
    for (uint8_t b : bytes) {
        hex.push_back(hexChars[(b >> 4) & 0x0F]);
        hex.push_back(hexChars[b & 0x0F]);
    }
    return hex;
}

bool constantTimeEquals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

} // namespace

SessionResumeManager::SessionResumeManager(std::string sessionSecret, SessionResumeConfig config) noexcept
    : sessionSecret_(std::move(sessionSecret)), config_(config) {}

std::string SessionResumeManager::signPayload(std::string_view payload, std::string_view secret) noexcept {
    auto hmac = computeHmacSha256(
        reinterpret_cast<const uint8_t*>(secret.data()), secret.size(),
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size()
    );
    return toHex(hmac);
}

common::Result<SessionResumeToken> SessionResumeManager::issueToken(
    const std::string& sessionId,
    const std::string& guestId,
    uint64_t currentEpochSec
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessionSecret_.empty()) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::NotInitialized,
                             "SessionResumeManager secret is not configured");
    }

    nonceCounter_++;
    uint64_t nonce = nonceCounter_;
    uint64_t expiresAt = currentEpochSec + config_.tokenValiditySec;

    std::ostringstream payloadStream;
    payloadStream << sessionId << ":" << guestId << ":" << currentEpochSec << ":" << expiresAt << ":" << nonce;
    std::string payload = payloadStream.str();

    std::string signature = signPayload(payload, sessionSecret_);

    std::ostringstream tokenStream;
    tokenStream << sessionId << "." << guestId << "." << currentEpochSec << "." << expiresAt << "." << nonce << "." << signature;
    std::string rawToken = tokenStream.str();

    SessionResumeToken token{};
    token.rawToken = rawToken;
    token.sessionId = sessionId;
    token.guestId = guestId;
    token.issuedAtSec = currentEpochSec;
    token.expiresAtSec = expiresAt;
    token.nonce = nonce;
    token.signatureHex = signature;

    activeTokens_[guestId] = token;
    return token;
}

common::Result<SessionResumeToken> SessionResumeManager::parseToken(const std::string& rawToken) noexcept {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos = rawToken.find('.');
    while (pos != std::string::npos) {
        parts.push_back(rawToken.substr(start, pos - start));
        start = pos + 1;
        pos = rawToken.find('.', start);
    }
    parts.push_back(rawToken.substr(start));

    if (parts.size() != 6) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::InvalidArgument,
                             "Malformed session resume token structure");
    }

    try {
        SessionResumeToken token{};
        token.rawToken = rawToken;
        token.sessionId = parts[0];
        token.guestId = parts[1];
        token.issuedAtSec = std::stoull(parts[2]);
        token.expiresAtSec = std::stoull(parts[3]);
        token.nonce = std::stoull(parts[4]);
        token.signatureHex = parts[5];
        return token;
    } catch (...) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::InvalidArgument,
                             "Invalid numeric fields in session resume token");
    }
}

common::Result<std::string> SessionResumeManager::validateAndConsumeToken(
    const std::string& rawToken,
    const std::string& guestId,
    uint64_t currentEpochSec
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessionSecret_.empty()) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::NotInitialized,
                             "Session secret not configured");
    }

    // Replay attack check: was this token already consumed?
    if (consumedTokens_.find(rawToken) != consumedTokens_.end()) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::AuthenticationFailed,
                             "Session resume token has already been consumed (replay attempt detected)");
    }

    auto parseRes = parseToken(rawToken);
    if (!parseRes.hasValue()) {
        return parseRes.error();
    }

    const auto& token = parseRes.value();

    if (token.guestId != guestId) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::AuthenticationFailed,
                             "Resume token guest ID mismatch");
    }

    if (token.isExpired(currentEpochSec)) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::Timeout,
                             "Session resume token expired");
    }

    // Recompute signature
    std::ostringstream payloadStream;
    payloadStream << token.sessionId << ":" << token.guestId << ":"
                  << token.issuedAtSec << ":" << token.expiresAtSec << ":" << token.nonce;
    std::string expectedSig = signPayload(payloadStream.str(), sessionSecret_);

    if (!constantTimeEquals(token.signatureHex, expectedSig)) {
        return common::Error(common::SubsystemId::Session, common::ErrorCode::AuthenticationFailed,
                             "Invalid cryptographic signature on resume token");
    }

    // Mark consumed to prevent replay attacks
    consumedTokens_.insert(rawToken);

    return token.sessionId;
}

void SessionResumeManager::invalidateGuestTokens(const std::string& guestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = activeTokens_.find(guestId);
    if (it != activeTokens_.end()) {
        consumedTokens_.insert(it->second.rawToken);
        activeTokens_.erase(it);
    }
}

void SessionResumeManager::invalidateAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [_, token] : activeTokens_) {
        consumedTokens_.insert(token.rawToken);
    }
    activeTokens_.clear();
}

} // namespace mist::session
