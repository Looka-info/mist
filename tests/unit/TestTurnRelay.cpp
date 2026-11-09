#include <gtest/gtest.h>
#include "mist/transport/TurnRelayManager.h"

using namespace mist::transport;

TEST(TurnRelayTest, EphemeralCredentialGenerationAndVerification) {
    std::string secret = "test-super-secret-key-12345";
    std::string peerId = "guest-pc-42";
    uint64_t nowEpoch = 1700000000;
    uint32_t ttl = 3600; // 1 hour

    auto [username, credential] = TurnCredentialGenerator::generateRestCredentials(secret, peerId, ttl, nowEpoch);

    // Format check: "<now+ttl>:<peerId>"
    EXPECT_EQ(username, "1700003600:guest-pc-42");
    EXPECT_FALSE(credential.empty());

    // Valid check within TTL
    EXPECT_TRUE(TurnCredentialGenerator::verifyRestCredentials(secret, username, credential, nowEpoch + 1800));

    // Expired check (1 second past expiry)
    EXPECT_FALSE(TurnCredentialGenerator::verifyRestCredentials(secret, username, credential, nowEpoch + 3601));

    // Tampered secret check
    EXPECT_FALSE(TurnCredentialGenerator::verifyRestCredentials("wrong-secret-key", username, credential, nowEpoch + 1800));

    // Tampered username check
    EXPECT_FALSE(TurnCredentialGenerator::verifyRestCredentials(secret, "1700003600:malicious-peer", credential, nowEpoch + 1800));
}

TEST(TurnRelayTest, TurnUriParsing) {
    // 1. Standard UDP TURN
    auto epUdp = TurnCredentialGenerator::parseTurnUri("turn:relay.mist.io:3478", "user", "pass");
    ASSERT_TRUE(epUdp.has_value());
    EXPECT_EQ(epUdp->host, "relay.mist.io");
    EXPECT_EQ(epUdp->port, 3478);
    EXPECT_EQ(epUdp->protocol, TurnProtocol::Udp);

    // 2. TCP TURN with query string
    auto epTcp = TurnCredentialGenerator::parseTurnUri("turn:relay.mist.io:3478?transport=tcp", "user", "pass");
    ASSERT_TRUE(epTcp.has_value());
    EXPECT_EQ(epTcp->host, "relay.mist.io");
    EXPECT_EQ(epTcp->port, 3478);
    EXPECT_EQ(epTcp->protocol, TurnProtocol::Tcp);

    // 3. TLS TURNS (RFC 5766 default 5349)
    auto epTls = TurnCredentialGenerator::parseTurnUri("turns:secure-relay.mist.io:5349", "user", "pass");
    ASSERT_TRUE(epTls.has_value());
    EXPECT_EQ(epTls->host, "secure-relay.mist.io");
    EXPECT_EQ(epTls->port, 5349);
    EXPECT_EQ(epTls->protocol, TurnProtocol::Tls);

    // 4. Default port when omitted
    auto epDefault = TurnCredentialGenerator::parseTurnUri("turn:relay.mist.io");
    ASSERT_TRUE(epDefault.has_value());
    EXPECT_EQ(epDefault->port, 3478);

    // 5. Invalid URI scheme
    EXPECT_FALSE(TurnCredentialGenerator::parseTurnUri("http://relay.mist.io").has_value());
    EXPECT_FALSE(TurnCredentialGenerator::parseTurnUri("stun:stun.l.google.com:19302").has_value());
}

TEST(TurnRelayTest, TurnRelayManagerLifecycleAndDiagnostics) {
    TurnRelayManager mgr;
    EXPECT_FALSE(mgr.isRelayAvailable());
    EXPECT_FALSE(mgr.isRelayInUse());

    uint64_t nowEpoch = 1700000000;
    auto ep1 = TurnCredentialGenerator::parseTurnUri("turn:relay.mist.io:3478", "user1", "pass1", nowEpoch + 3600);
    auto ep2 = TurnCredentialGenerator::parseTurnUri("turns:secure.mist.io:5349", "user2", "pass2", nowEpoch + 1800);
    ASSERT_TRUE(ep1.has_value() && ep2.has_value());

    (void)mgr.configureRelays({*ep1, *ep2});
    EXPECT_TRUE(mgr.isRelayAvailable());
    EXPECT_FALSE(mgr.isRelayInUse());

    // Both valid at nowEpoch
    auto activeIce = mgr.getActiveIceServers(nowEpoch);
    EXPECT_EQ(activeIce.size(), 2);

    // At nowEpoch + 2000, ep2 expired (expiresAt = nowEpoch + 1800)
    auto activeLater = mgr.getActiveIceServers(nowEpoch + 2000);
    EXPECT_EQ(activeLater.size(), 1);
    EXPECT_EQ(activeLater[0].url, "turn:relay.mist.io:3478");

    // Simulate engaging relay fallback
    mgr.updateRelayState(RelayState::ActiveInUse, "turn:relay.mist.io:3478", TurnProtocol::Udp);
    EXPECT_TRUE(mgr.isRelayInUse());

    // Record metrics: direct RTT was 5ms, relay RTT is 28ms -> 23ms penalty
    mgr.recordRelayMetrics(28.0, 5.0, 500000);
    auto diag = mgr.getDiagnostics();
    EXPECT_TRUE(diag.isRelayed);
    EXPECT_DOUBLE_EQ(diag.directRttMs, 5.0);
    EXPECT_DOUBLE_EQ(diag.relayRttMs, 28.0);
    EXPECT_DOUBLE_EQ(diag.latencyPenaltyMs, 23.0);
    EXPECT_EQ(diag.bytesRelayed, 500000);

    // Failure simulation
    mgr.recordRelayFailure("Relay connection timed out");
    EXPECT_FALSE(mgr.isRelayInUse());
    EXPECT_EQ(mgr.getDiagnostics().state, RelayState::Failed);
    EXPECT_EQ(mgr.getDiagnostics().failureReason, "Relay connection timed out");
}
