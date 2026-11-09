#include <gtest/gtest.h>
#include "mist/transport/NatTraversal.h"
#include <thread>

using namespace mist::transport;

TEST(NatTraversalTest, Rfc8445PriorityOrdering) {
    uint32_t hostIpv6Prio = IceCandidatePrioritizer::computeRfc8445Priority(CandidateType::Host, AddressFamily::IPv6, 1);
    uint32_t hostIpv4Prio = IceCandidatePrioritizer::computeRfc8445Priority(CandidateType::Host, AddressFamily::IPv4, 1);
    uint32_t prflxPrio = IceCandidatePrioritizer::computeRfc8445Priority(CandidateType::PeerReflexive, AddressFamily::IPv4, 1);
    uint32_t srflxPrio = IceCandidatePrioritizer::computeRfc8445Priority(CandidateType::ServerReflexive, AddressFamily::IPv4, 1);
    uint32_t relayPrio = IceCandidatePrioritizer::computeRfc8445Priority(CandidateType::Relayed, AddressFamily::IPv4, 1);

    // RFC 8445 Section 5.1.2: Host > Prflx > Srflx > Relay
    EXPECT_GT(hostIpv4Prio, prflxPrio);
    EXPECT_GT(prflxPrio, srflxPrio);
    EXPECT_GT(srflxPrio, relayPrio);

    // RFC 6724 / 8445: IPv6 local preference > IPv4 local preference
    EXPECT_GT(hostIpv6Prio, hostIpv4Prio);
}

TEST(NatTraversalTest, SdpCandidateParsing) {
    // 1. Host IPv4 candidate
    std::string lineHost = "candidate:1 1 UDP 2130706431 192.168.1.150 54123 typ host";
    auto parsedHost = IceCandidatePrioritizer::parseSdpCandidate(lineHost);
    ASSERT_TRUE(parsedHost.has_value());
    EXPECT_EQ(parsedHost->type, CandidateType::Host);
    EXPECT_EQ(parsedHost->ip, "192.168.1.150");
    EXPECT_EQ(parsedHost->port, 54123);
    EXPECT_EQ(parsedHost->family, AddressFamily::IPv4);
    EXPECT_TRUE(parsedHost->isPrivateLan);
    EXPECT_FALSE(parsedHost->isCgnat);

    // 2. Server-reflexive candidate with raddr and rport
    std::string lineSrflx = "candidate:2 1 UDP 1694498815 203.0.113.50 62100 typ srflx raddr 192.168.1.150 rport 54123";
    auto parsedSrflx = IceCandidatePrioritizer::parseSdpCandidate(lineSrflx);
    ASSERT_TRUE(parsedSrflx.has_value());
    EXPECT_EQ(parsedSrflx->type, CandidateType::ServerReflexive);
    EXPECT_EQ(parsedSrflx->ip, "203.0.113.50");
    EXPECT_EQ(parsedSrflx->port, 62100);
    EXPECT_EQ(parsedSrflx->relatedIp, "192.168.1.150");
    EXPECT_EQ(parsedSrflx->relatedPort, 54123);
    EXPECT_FALSE(parsedSrflx->isPrivateLan);

    // 3. IPv6 Host candidate
    std::string lineIpv6 = "candidate:3 1 UDP 2130706432 2001:db8::1 54123 typ host";
    auto parsedIpv6 = IceCandidatePrioritizer::parseSdpCandidate(lineIpv6);
    ASSERT_TRUE(parsedIpv6.has_value());
    EXPECT_EQ(parsedIpv6->type, CandidateType::Host);
    EXPECT_EQ(parsedIpv6->family, AddressFamily::IPv6);

    // 4. Malformed candidates
    EXPECT_FALSE(IceCandidatePrioritizer::parseSdpCandidate("invalid-candidate-string").has_value());
    EXPECT_FALSE(IceCandidatePrioritizer::parseSdpCandidate("candidate:1 1").has_value());
}

TEST(NatTraversalTest, AddressClassificationAndCgnat) {
    // CGNAT range RFC 6598: 100.64.0.0/10 (100.64.0.0 to 100.127.255.255)
    EXPECT_TRUE(IceCandidatePrioritizer::isCgnatAddress("100.64.0.1"));
    EXPECT_TRUE(IceCandidatePrioritizer::isCgnatAddress("100.100.50.25"));
    EXPECT_TRUE(IceCandidatePrioritizer::isCgnatAddress("100.127.255.254"));
    EXPECT_FALSE(IceCandidatePrioritizer::isCgnatAddress("100.63.255.255"));
    EXPECT_FALSE(IceCandidatePrioritizer::isCgnatAddress("100.128.0.1"));
    EXPECT_FALSE(IceCandidatePrioritizer::isCgnatAddress("192.168.1.1"));

    // Private LAN ranges RFC 1918
    EXPECT_TRUE(IceCandidatePrioritizer::isPrivateAddress("10.0.0.1"));
    EXPECT_TRUE(IceCandidatePrioritizer::isPrivateAddress("172.16.0.1"));
    EXPECT_TRUE(IceCandidatePrioritizer::isPrivateAddress("172.31.255.255"));
    EXPECT_TRUE(IceCandidatePrioritizer::isPrivateAddress("192.168.0.1"));
    EXPECT_TRUE(IceCandidatePrioritizer::isPrivateAddress("169.254.10.20"));
    EXPECT_FALSE(IceCandidatePrioritizer::isPrivateAddress("8.8.8.8"));
    EXPECT_FALSE(IceCandidatePrioritizer::isPrivateAddress("1.1.1.1"));

    // Loopback
    EXPECT_TRUE(IceCandidatePrioritizer::isLoopbackAddress("127.0.0.1"));
    EXPECT_TRUE(IceCandidatePrioritizer::isLoopbackAddress("::1"));
    EXPECT_FALSE(IceCandidatePrioritizer::isLoopbackAddress("192.168.1.1"));
}

TEST(NatTraversalTest, NatBehaviorClassification) {
    // 1. Direct open Internet
    std::vector<ParsedIceCandidate> openCandidates;
    auto cOpen = IceCandidatePrioritizer::parseSdpCandidate("candidate:1 1 UDP 2130706431 203.0.113.10 5000 typ host");
    ASSERT_TRUE(cOpen.has_value());
    openCandidates.push_back(*cOpen);
    EXPECT_EQ(IceCandidatePrioritizer::classifyNatBehavior(openCandidates), NatType::OpenInternetDirect);

    // 2. CGNAT detection
    std::vector<ParsedIceCandidate> cgnatCandidates;
    auto cHost = IceCandidatePrioritizer::parseSdpCandidate("candidate:1 1 UDP 2130706431 192.168.1.50 5000 typ host");
    auto cSrflxCgnat = IceCandidatePrioritizer::parseSdpCandidate("candidate:2 1 UDP 1694498815 100.70.1.2 5000 typ srflx");
    ASSERT_TRUE(cHost.has_value() && cSrflxCgnat.has_value());
    cgnatCandidates.push_back(*cHost);
    cgnatCandidates.push_back(*cSrflxCgnat);
    EXPECT_EQ(IceCandidatePrioritizer::classifyNatBehavior(cgnatCandidates), NatType::CarrierGradeNat);

    // 3. Full Cone (Port preserved)
    std::vector<ParsedIceCandidate> fullConeCandidates;
    auto cSrflxPreserved = IceCandidatePrioritizer::parseSdpCandidate("candidate:2 1 UDP 1694498815 203.0.113.50 5000 typ srflx");
    ASSERT_TRUE(cSrflxPreserved.has_value());
    fullConeCandidates.push_back(*cHost);
    fullConeCandidates.push_back(*cSrflxPreserved);
    EXPECT_EQ(IceCandidatePrioritizer::classifyNatBehavior(fullConeCandidates), NatType::FullCone);
}

TEST(NatTraversalTest, ConnectionFailureClassification) {
    // Signaling down
    EXPECT_EQ(IceCandidatePrioritizer::classifyFailure(false, false, false, false), ConnectionFailureReason::SignalingUnreachable);

    // Candidates gathering failed
    EXPECT_EQ(IceCandidatePrioritizer::classifyFailure(true, false, false, false), ConnectionFailureReason::IceGatheringFailed);

    // Symmetric NAT on both ends prevented direct hole punch
    EXPECT_EQ(IceCandidatePrioritizer::classifyFailure(true, true, false, true), ConnectionFailureReason::SymmetricNatMismatch);

    // General ICE connectivity check timeout
    EXPECT_EQ(IceCandidatePrioritizer::classifyFailure(true, true, false, false), ConnectionFailureReason::IceConnectivityTimeout);

    // Connected successfully
    EXPECT_EQ(IceCandidatePrioritizer::classifyFailure(true, true, true, false), ConnectionFailureReason::None);
}

TEST(NatTraversalTest, PortMappingLifecycle) {
    PortMappingManager mgr;
    EXPECT_EQ(mgr.getActiveLeases().size(), 0);

    // Request temporary mapping
    auto leaseRes = mgr.requestPortMapping(54000, 54000, "UDP", 1800, "Mist Unit Test");
    ASSERT_TRUE(leaseRes.hasValue());
    EXPECT_EQ(leaseRes.value().internalPort, 54000);
    EXPECT_EQ(leaseRes.value().externalPort, 54000);
    EXPECT_TRUE(leaseRes.value().isActive);
    EXPECT_TRUE(mgr.hasActiveLease(54000));
    EXPECT_EQ(mgr.getActiveLeases().size(), 1);

    // Release specific port mapping
    auto relRes = mgr.releasePortMapping(54000, "UDP");
    EXPECT_TRUE(relRes.hasValue());
    EXPECT_FALSE(mgr.hasActiveLease(54000));
    EXPECT_EQ(mgr.getActiveLeases().size(), 0);

    // Multiple leases and bulk cleanup
    (void)mgr.requestPortMapping(54001, 54001, "UDP", 3600);
    (void)mgr.requestPortMapping(54002, 54002, "UDP", 3600);
    EXPECT_EQ(mgr.getActiveLeases().size(), 2);
    mgr.releaseAllMappings();
    EXPECT_EQ(mgr.getActiveLeases().size(), 0);
}

TEST(NatTraversalTest, IceRestartRateLimitingAndBackoff) {
    IceRestartController ctrl(3);
    EXPECT_EQ(ctrl.getAttemptCount(), 0);
    EXPECT_EQ(ctrl.getGeneration(), 0);
    EXPECT_TRUE(ctrl.canAttemptRestart());

    // First attempt
    ctrl.recordRestartAttempt();
    EXPECT_EQ(ctrl.getAttemptCount(), 1);
    EXPECT_EQ(ctrl.getGeneration(), 1);

    // Immediate second attempt should be rate-limited (< 1000ms)
    EXPECT_FALSE(ctrl.canAttemptRestart());

    // Simulate waiting 1050ms
    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    EXPECT_TRUE(ctrl.canAttemptRestart());

    ctrl.recordRestartAttempt();
    EXPECT_EQ(ctrl.getAttemptCount(), 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    ctrl.recordRestartAttempt();
    EXPECT_EQ(ctrl.getAttemptCount(), 3);

    // Exceeded maxRestarts (3)
    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    EXPECT_FALSE(ctrl.canAttemptRestart());

    // Reset restores capability
    ctrl.reset();
    EXPECT_EQ(ctrl.getAttemptCount(), 0);
    EXPECT_TRUE(ctrl.canAttemptRestart());
}
