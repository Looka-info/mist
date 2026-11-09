#include <gtest/gtest.h>
#include "mist/security/PeerIdentityVerifier.h"

using namespace mist::security;

class PeerIdentityTest : public ::testing::Test {};

TEST_F(PeerIdentityTest, ExtractFingerprintFromSdp) {
    const std::string sdpWithFingerprint =
        "v=0\r\n"
        "o=- 38291029 2 IN IP4 127.0.0.1\r\n"
        "s=-\r\n"
        "t=0 0\r\n"
        "a=fingerprint:sha-256 4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB:D5:08:05:3A:1E:0A:C4:4E:57:3E:45:9E\r\n"
        "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n";

    std::string extracted = PeerIdentityVerifier::extractFingerprintFromSdp(sdpWithFingerprint);
    EXPECT_EQ(extracted, "4AADB9B13F82183B540212DF3E5D496B19E57CABD508053A1E0AC44E573E459E");

    const std::string sdpWithoutFingerprint =
        "v=0\r\n"
        "o=- 12345 2 IN IP4 127.0.0.1\r\n"
        "s=-\r\n";
    EXPECT_TRUE(PeerIdentityVerifier::extractFingerprintFromSdp(sdpWithoutFingerprint).empty());
}

TEST_F(PeerIdentityTest, FingerprintVerificationAndMismatch) {
    PeerIdentityVerifier verifier;
    const std::string expectedFp = "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB:D5:08:05:3A:1E:0A:C4:4E:57:3E:45:9E";
    verifier.setExpectedRemoteFingerprint(expectedFp);
    verifier.setSessionContext("session-sec-1", "auth-token-xyz");

    // 1. Matching SDP verifies successfully
    const std::string validSdp =
        "v=0\r\n"
        "a=fingerprint:sha-256 4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB:D5:08:05:3A:1E:0A:C4:4E:57:3E:45:9E\r\n";

    auto resValid = verifier.verifyRemoteDescription(validSdp);
    EXPECT_TRUE(resValid.hasValue());
    EXPECT_EQ(verifier.getTrustState(), IdentityTrustState::Verified);

    // 2. Mismatched SDP is rejected
    verifier.reset();
    verifier.setExpectedRemoteFingerprint(expectedFp);

    const std::string forgedSdp =
        "v=0\r\n"
        "a=fingerprint:sha-256 FF:FF:FF:FF:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB:D5:08:05:3A:1E:0A:C4:4E:57:3E:45:9E\r\n";

    auto resForged = verifier.verifyRemoteDescription(forgedSdp);
    EXPECT_FALSE(resForged.hasValue());
    EXPECT_EQ(verifier.getTrustState(), IdentityTrustState::FingerprintMismatch);
    EXPECT_TRUE(resForged.error().message().find("mismatch") != std::string::npos);
}

TEST_F(PeerIdentityTest, UnexpectedIdentityChangeInFlight) {
    PeerIdentityVerifier verifier;
    const std::string sdpA =
        "v=0\r\n"
        "a=fingerprint:sha-256 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11\r\n";
    const std::string sdpB =
        "v=0\r\n"
        "a=fingerprint:sha-256 22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22:22\r\n";

    // First description accepted
    auto res1 = verifier.verifyRemoteDescription(sdpA);
    EXPECT_TRUE(res1.hasValue());
    EXPECT_EQ(verifier.getTrustState(), IdentityTrustState::Verified);

    // Subsequent description with different certificate triggers IdentityChanged alarm
    auto res2 = verifier.verifyRemoteDescription(sdpB);
    EXPECT_FALSE(res2.hasValue());
    EXPECT_EQ(verifier.getTrustState(), IdentityTrustState::IdentityChanged);
    EXPECT_TRUE(res2.error().message().find("changed during active session") != std::string::npos);
}

TEST_F(PeerIdentityTest, ShortAuthenticationStringComputation) {
    const std::string hostFp = "11:22:33:44:55:66:77:88";
    const std::string guestFp = "AA:BB:CC:DD:EE:FF:00:11";
    const std::string sessionId = "mist-sess-42";
    const std::string token = "mist-auth-secret-123";

    std::string sas1 = PeerIdentityVerifier::calculateSas(hostFp, guestFp, sessionId, token);
    std::string sas2 = PeerIdentityVerifier::calculateSas(hostFp, guestFp, sessionId, token);

    // SAS format is XXX-YYY (7 characters)
    EXPECT_EQ(sas1.size(), 7u);
    EXPECT_EQ(sas1[3], '-');
    EXPECT_EQ(sas1, sas2); // Deterministic

    // Any change in identity material alters the SAS
    std::string forgedSas = PeerIdentityVerifier::calculateSas(
        "99:99:99:99:55:66:77:88", // MitM altered host fingerprint
        guestFp,
        sessionId,
        token
    );
    EXPECT_NE(sas1, forgedSas);
}
