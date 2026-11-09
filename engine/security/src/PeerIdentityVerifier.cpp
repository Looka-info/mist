#include "mist/security/PeerIdentityVerifier.h"
#include "mist/security/Security.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <array>
#include <vector>

namespace mist::security {

namespace {

// Pure C++ standard SHA-256 implementation (FIPS 180-4) for zero-dependency cryptographic hashing
std::array<uint8_t, 32> computeSha256(const uint8_t* data, size_t len) noexcept {
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

    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    std::vector<uint8_t> msg(data, data + len);
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xff));
    }

    auto rotr = [](uint32_t x, uint32_t n) noexcept {
        return (x >> n) | (x << (32 - n));
    };

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[64];
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
        }
        for (size_t i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], h_var = h[7];

        for (size_t i = 0; i < 64; ++i) {
            uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h_var + s1 + ch + K[i] + w[i];
            uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = s0 + maj;

            h_var = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += h_var;
    }

    std::array<uint8_t, 32> out{};
    for (size_t i = 0; i < 8; ++i) {
        out[i * 4]     = static_cast<uint8_t>((h[i] >> 24) & 0xff);
        out[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xff);
        out[i * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xff);
        out[i * 4 + 3] = static_cast<uint8_t>(h[i] & 0xff);
    }
    return out;
}

} // namespace

std::string PeerIdentityVerifier::normalizeFingerprint(std::string_view fp) {
    std::string norm;
    norm.reserve(fp.size());
    for (char c : fp) {
        if (c != ':' && c != ' ' && c != '-') {
            norm.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return norm;
}

std::string PeerIdentityVerifier::extractFingerprintFromSdp(std::string_view sdp) {
    // SDP attribute format: a=fingerprint:<hash-func> <fingerprint>
    const std::string_view prefix = "a=fingerprint:";
    size_t pos = 0;

    while (pos < sdp.size()) {
        size_t lineEnd = sdp.find('\n', pos);
        if (lineEnd == std::string_view::npos) {
            lineEnd = sdp.size();
        }

        std::string_view line = sdp.substr(pos, lineEnd - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (line.starts_with(prefix)) {
            // Found fingerprint line, e.g. "a=fingerprint:sha-256 4A:AD:B9:..."
            std::string_view value = line.substr(prefix.size());
            size_t spacePos = value.find(' ');
            if (spacePos != std::string_view::npos) {
                return normalizeFingerprint(value.substr(spacePos + 1));
            }
        }

        pos = lineEnd + 1;
    }

    return "";
}

std::string PeerIdentityVerifier::calculateSas(
    std::string_view hostFp,
    std::string_view guestFp,
    std::string_view sessionId,
    std::string_view authToken
) {
    std::string normHost = normalizeFingerprint(hostFp);
    std::string normGuest = normalizeFingerprint(guestFp);

    // Concatenate security material
    std::string material = normHost + "|" + normGuest + "|" + std::string(sessionId) + "|" + std::string(authToken);
    auto digest = computeSha256(reinterpret_cast<const uint8_t*>(material.data()), material.size());

    // Derive 6-digit numeric SAS (000000 - 999999) from first 4 bytes
    uint32_t val = (static_cast<uint32_t>(digest[0]) << 24) |
                   (static_cast<uint32_t>(digest[1]) << 16) |
                   (static_cast<uint32_t>(digest[2]) << 8) |
                   (static_cast<uint32_t>(digest[3]));
    uint32_t sasNum = val % 1000000;

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%03u-%03u", sasNum / 1000, sasNum % 1000);
    return std::string(buf);
}

void PeerIdentityVerifier::setExpectedRemoteFingerprint(std::string expectedFp) {
    std::lock_guard<std::mutex> lock(mutex_);
    expectedRemoteFingerprint_ = normalizeFingerprint(expectedFp);
}

void PeerIdentityVerifier::setLocalFingerprint(std::string localFp) {
    std::lock_guard<std::mutex> lock(mutex_);
    localFingerprint_ = normalizeFingerprint(localFp);
    updateSasUnderLock();
}

void PeerIdentityVerifier::setSessionContext(std::string sessionId, std::string authToken) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessionId_ = std::move(sessionId);
    authToken_ = std::move(authToken);
    updateSasUnderLock();
}

void PeerIdentityVerifier::updateSasUnderLock() {
    if (!localFingerprint_.empty() && !activeRemoteFingerprint_.empty()) {
        shortAuthString_ = calculateSas(localFingerprint_, activeRemoteFingerprint_, sessionId_, authToken_);
    }
}

common::Result<void> PeerIdentityVerifier::verifyRemoteDescription(const std::string& sdp) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string extracted = extractFingerprintFromSdp(sdp);
    if (extracted.empty()) {
        trustState_ = IdentityTrustState::NoFingerprintInSdp;
        statusMessage_ = "No DTLS certificate fingerprint present in remote SDP.";
        return common::Error(
            common::SubsystemId::Security,
            common::ErrorCode::AuthenticationFailed,
            statusMessage_
        );
    }

    // Check for in-flight unexpected identity change
    if (!activeRemoteFingerprint_.empty() && !constantTimeEquals(activeRemoteFingerprint_, extracted)) {
        trustState_ = IdentityTrustState::IdentityChanged;
        statusMessage_ = "CRITICAL: Remote peer certificate fingerprint changed during active session! Potential MitM or session hijacking.";
        return common::Error(
            common::SubsystemId::Security,
            common::ErrorCode::AuthenticationFailed,
            statusMessage_
        );
    }

    // Check against expected fingerprint if set
    if (!expectedRemoteFingerprint_.empty()) {
        if (!constantTimeEquals(expectedRemoteFingerprint_, extracted)) {
            trustState_ = IdentityTrustState::FingerprintMismatch;
            statusMessage_ = "DTLS certificate fingerprint mismatch! Expected: " + expectedRemoteFingerprint_ + ", Received: " + extracted;
            return common::Error(
                common::SubsystemId::Security,
                common::ErrorCode::AuthenticationFailed,
                statusMessage_
            );
        }
    }

    activeRemoteFingerprint_ = extracted;
    trustState_ = IdentityTrustState::Verified;
    statusMessage_ = "Peer DTLS certificate verified.";
    updateSasUnderLock();

    return common::Result<void>::success();
}

SecurityDiagnostics PeerIdentityVerifier::getDiagnostics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SecurityDiagnostics d;
    d.isEncrypted = true;
    d.cipherSuite = "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
    d.localFingerprint = localFingerprint_;
    d.remoteFingerprint = activeRemoteFingerprint_;
    d.expectedFingerprint = expectedRemoteFingerprint_;
    d.shortAuthString = shortAuthString_;
    d.trustState = trustState_;
    d.statusMessage = statusMessage_;
    return d;
}

IdentityTrustState PeerIdentityVerifier::getTrustState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return trustState_;
}

std::string PeerIdentityVerifier::getShortAuthString() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shortAuthString_;
}

void PeerIdentityVerifier::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    activeRemoteFingerprint_.clear();
    shortAuthString_.clear();
    trustState_ = IdentityTrustState::Unverified;
    statusMessage_ = "Unverified";
}

} // namespace mist::security
