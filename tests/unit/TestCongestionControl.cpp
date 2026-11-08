#include <gtest/gtest.h>
#include "mist/transport/qos/QosTypes.h"
#include "mist/transport/qos/NetworkQualityEstimator.h"
#include "mist/transport/qos/CongestionController.h"
#include "mist/transport/qos/QualityAdaptationManager.h"
#include "mist/transport/qos/AdaptiveBitrateController.h"

using namespace mist::transport::qos;

TEST(QosTest, NetworkQualityEstimatorSmoothing) {
    NetworkQualityEstimator estimator;
    EXPECT_EQ(estimator.getConnectionType(), ConnectionType::Unknown);

    // Initial RTCP report
    RtcpStatistics rtcp1{};
    rtcp1.fractionLost = 26; // ~10.1% loss
    rtcp1.roundTripTimeMs = 20.0;
    rtcp1.jitterSamples = 90; // 1.0 ms
    estimator.updateRtcpStats(rtcp1);

    EXPECT_NEAR(estimator.getSmoothedLossPercent(), 10.15, 0.1);
    EXPECT_NEAR(estimator.getSmoothedRttMs(), 20.0, 0.1);

    // Second RTCP report with lower RTT
    RtcpStatistics rtcp2{};
    rtcp2.fractionLost = 0;
    rtcp2.roundTripTimeMs = 10.0;
    estimator.updateRtcpStats(rtcp2);

    // Loss EWMA: 0.8 * 10.15 + 0.2 * 0 = ~8.12%
    EXPECT_LT(estimator.getSmoothedLossPercent(), 10.15);
    // RTT EWMA: 0.85 * 20.0 + 0.15 * 10.0 = 18.5 ms
    EXPECT_NEAR(estimator.getSmoothedRttMs(), 18.5, 0.1);
}

TEST(QosTest, CandidatePairClassification) {
    NetworkQualityEstimator estimator;
    estimator.updateSelectedCandidatePair("host", "host");
    EXPECT_EQ(estimator.getConnectionType(), ConnectionType::DirectLan);

    estimator.updateSelectedCandidatePair("srflx", "host");
    EXPECT_EQ(estimator.getConnectionType(), ConnectionType::DirectWan);

    estimator.updateSelectedCandidatePair("relay", "host");
    EXPECT_EQ(estimator.getConnectionType(), ConnectionType::RelayedTurn);
}

TEST(QosTest, CongestionControllerStateMachine) {
    ProfileConfig cfg{};
    cfg.minBitrateKbps = 2000;
    cfg.maxBitrateKbps = 15000;
    cfg.initialBitrateKbps = 8000;
    cfg.lossThresholdPercent = 2.5;
    cfg.maxAcceptableRttMs = 60.0;

    CongestionController controller(cfg);
    EXPECT_EQ(controller.getState(), CongestionState::Stable);
    EXPECT_EQ(controller.getCurrentBitrateKbps(), 8000);

    uint64_t t = 1000;

    // 1. Stable network conditions -> probes upwards (Additive Increase)
    uint32_t b1 = controller.processFeedback(20.0, 0.0, 0.5, 20000000, t);
    EXPECT_GT(b1, 8000);
    EXPECT_EQ(controller.getState(), CongestionState::Increasing);

    // 2. High packet loss (5%) -> Persistent congestion (Multiplicative Decrease)
    t += 1100;
    uint32_t b2 = controller.processFeedback(25.0, 5.0, 1.0, 20000000, t);
    EXPECT_LT(b2, b1);
    EXPECT_EQ(controller.getState(), CongestionState::Congested);

    // 3. Hysteresis test: subsequent loss within 500ms does not cause immediate second backoff
    t += 300;
    uint32_t b3 = controller.processFeedback(25.0, 5.0, 1.0, 20000000, t);
    EXPECT_EQ(b3, b2);

    // 4. Critical congestion: severe packet loss (20%) -> immediate drop
    t += 1200;
    uint32_t b4 = controller.processFeedback(150.0, 20.0, 5.0, 20000000, t);
    EXPECT_EQ(controller.getState(), CongestionState::CriticallyCongested);
    EXPECT_LE(b4, b2 / 2);
    EXPECT_GE(b4, cfg.minBitrateKbps);
}

TEST(QosTest, QualityAdaptationManagerBudgets) {
    QualityAdaptationManager manager(QualityProfile::Balanced);
    EXPECT_EQ(manager.getProfile(), QualityProfile::Balanced);

    ProfileConfig cfg = manager.getProfileConfig();
    EXPECT_EQ(cfg.width, 1920);
    EXPECT_EQ(cfg.height, 1080);
    EXPECT_EQ(cfg.targetFps, 60);

    // Calculate budget for 8,000 kbps total
    BandwidthBudget budget = manager.calculateBudget(8000);
    EXPECT_EQ(budget.totalBandwidthKbps, 8000);
    EXPECT_EQ(budget.audioBitrateKbps, 128);
    EXPECT_EQ(budget.controlBitrateKbps, 50);
    EXPECT_GT(budget.transportHeadroomKbps, 150);
    EXPECT_EQ(budget.videoBitrateKbps, 8000 - 128 - 50 - budget.transportHeadroomKbps);
}

TEST(QosTest, AdaptiveBitrateControllerReconfigTrigger) {
    AdaptiveBitrateController controller(QualityProfile::Adaptive);

    uint32_t reconfigBitrate = 0;
    int callbackCount = 0;

    controller.setEncoderReconfigCallback([&](uint32_t newBitrateKbps) {
        reconfigBitrate = newBitrateKbps;
        callbackCount++;
        return mist::common::Result<void>::success();
    });

    uint64_t t = 1000;
    // Initial tick
    (void)controller.tick(t);

    // Inject severe network loss (18%)
    t += 600;
    RtcpStatistics severeRtcp{};
    severeRtcp.fractionLost = 46; // ~18% loss
    severeRtcp.roundTripTimeMs = 120.0;
    controller.onRtcpReportReceived(severeRtcp, t);

    // Reconfiguration callback should have been invoked due to urgent congestion
    EXPECT_GE(callbackCount, 1);
    EXPECT_LT(reconfigBitrate, 8000);
    EXPECT_EQ(controller.getCongestionState(), CongestionState::CriticallyCongested);
}

TEST(QosTest, ControlledNetworkImpairmentSimulation) {
    // Milestone 7 Workstream 2: Controlled network impairment simulation across
    // 25M, 10M, 5M, 2M, RTT spikes, packet loss bursts, and additive recovery.
    AdaptiveBitrateController controller(QualityProfile::Adaptive);

    std::vector<std::pair<uint64_t, uint32_t>> adaptationHistory;
    uint64_t currentSimTimeMs = 10000;
    controller.setEncoderReconfigCallback([&](uint32_t newBitrateKbps) {
        adaptationHistory.emplace_back(currentSimTimeMs, newBitrateKbps);
        return mist::common::Result<void>::success();
    });

    uint64_t t = 10000;
    currentSimTimeMs = t;

    // 1. Initial 25 Mbps high-bandwidth link with pristine metrics
    controller.onRembReceived(25000000, t); // 25 Mbps
    RtcpStatistics rtcpPristine{};
    rtcpPristine.fractionLost = 0;
    rtcpPristine.jitterSamples = 45;
    rtcpPristine.roundTripTimeMs = 8.5;
    controller.onRtcpReportReceived(rtcpPristine, t);
    uint32_t bInitial = controller.getCurrentVideoBitrateKbps();
    EXPECT_GE(bInitial, 2000);

    // 2. Network throttled to 10 Mbps
    t += 1000;
    currentSimTimeMs = t;
    controller.onRembReceived(10000000, t); // 10 Mbps
    controller.onRtcpReportReceived(rtcpPristine, t);
    uint32_t b10M = controller.getCurrentVideoBitrateKbps();
    EXPECT_LE(b10M, 10000);

    // 3. Network throttled to 5 Mbps with mild RTT elevation
    t += 1000;
    currentSimTimeMs = t;
    controller.onRembReceived(5000000, t); // 5 Mbps
    RtcpStatistics rtcpMild{};
    rtcpMild.fractionLost = 5;
    rtcpMild.jitterSamples = 90;
    rtcpMild.roundTripTimeMs = 35.0;
    controller.onRtcpReportReceived(rtcpMild, t);
    uint32_t b5M = controller.getCurrentVideoBitrateKbps();
    EXPECT_LE(b5M, 5000);

    // 4. Congestion Detection: Sudden loss burst (18%) and RTT spike (140 ms)
    t += 1000;
    currentSimTimeMs = t;
    RtcpStatistics rtcpBurst{};
    rtcpBurst.fractionLost = 46;
    rtcpBurst.jitterSamples = 450;
    rtcpBurst.roundTripTimeMs = 140.0;
    controller.onRtcpReportReceived(rtcpBurst, t);
    auto st = controller.getCongestionState();
    EXPECT_TRUE(st == CongestionState::Congested || st == CongestionState::CriticallyCongested);
    uint32_t bCritical = controller.getCurrentVideoBitrateKbps();
    EXPECT_LT(bCritical, b5M);

    // 5. Constrained 2 Mbps bottleneck
    t += 1000;
    currentSimTimeMs = t;
    controller.onRembReceived(2000000, t); // 2 Mbps
    controller.onRtcpReportReceived(rtcpBurst, t);
    uint32_t bBottleneck = controller.getCurrentVideoBitrateKbps();
    EXPECT_LE(bBottleneck, 3000);

    // 6. Network stabilization: EWMA decay with successive 0% loss reports
    RtcpStatistics rtcpRecovered{};
    rtcpRecovered.fractionLost = 0;
    rtcpRecovered.jitterSamples = 30;
    rtcpRecovered.roundTripTimeMs = 12.0;
    controller.onRembReceived(15000000, t); // 15 Mbps available again

    for (int i = 0; i < 8; ++i) {
        t += 600;
        currentSimTimeMs = t;
        controller.onRtcpReportReceived(rtcpRecovered, t);
    }
    uint32_t bTrough = controller.getCurrentVideoBitrateKbps();

    // 7. Controlled Additive Increase recovery
    t += 1200;
    currentSimTimeMs = t;
    (void)controller.tick(t);
    t += 1200;
    currentSimTimeMs = t;
    (void)controller.tick(t);
    uint32_t bRecovered = controller.getCurrentVideoBitrateKbps();
    EXPECT_GT(bRecovered, bTrough);
    EXPECT_EQ(controller.getCongestionState(), CongestionState::Increasing);
}
