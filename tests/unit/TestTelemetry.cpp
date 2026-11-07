#include <gtest/gtest.h>
#include "mist/telemetry/Metrics.h"

using namespace mist::telemetry;

TEST(TelemetryTest, CounterBehavior) {
    Counter counter("frames_rendered");
    EXPECT_EQ(counter.get(), 0u);
    EXPECT_EQ(counter.name(), "frames_rendered");

    counter.increment();
    EXPECT_EQ(counter.get(), 1u);

    counter.increment(59);
    EXPECT_EQ(counter.get(), 60u);
}

TEST(TelemetryTest, GaugeBehavior) {
    Gauge gauge("encode_time_ms");
    EXPECT_DOUBLE_EQ(gauge.get(), 0.0);

    gauge.set(2.45);
    EXPECT_DOUBLE_EQ(gauge.get(), 2.45);

    gauge.set(1.10);
    EXPECT_DOUBLE_EQ(gauge.get(), 1.10);
}

TEST(TelemetryTest, LatencyHistogramStatistics) {
    LatencyHistogram hist("frame_latency_us", 100);

    auto emptySnap = hist.snapshot();
    EXPECT_EQ(emptySnap.count, 0u);
    EXPECT_DOUBLE_EQ(emptySnap.avgUs, 0.0);

    for (int i = 1; i <= 100; ++i) {
        hist.record(static_cast<double>(i * 100)); // 100us, 200us, ..., 10000us
    }

    auto snap = hist.snapshot();
    EXPECT_EQ(snap.count, 100u);
    EXPECT_DOUBLE_EQ(snap.minUs, 100.0);
    EXPECT_DOUBLE_EQ(snap.maxUs, 10000.0);
    EXPECT_NEAR(snap.avgUs, 5050.0, 1.0);
    EXPECT_NEAR(snap.p50Us, 5000.0, 150.0);
    EXPECT_NEAR(snap.p95Us, 9500.0, 150.0);
}

TEST(TelemetryTest, MetricRegistryLifecycle) {
    auto& reg = MetricRegistry::instance();
    reg.resetAll();

    auto c1 = reg.getOrCreateCounter("network_packets");
    auto c2 = reg.getOrCreateCounter("network_packets");
    EXPECT_EQ(c1.get(), c2.get());

    c1->increment(10);
    EXPECT_EQ(c2->get(), 10u);

    auto g1 = reg.getOrCreateGauge("bitrate_mbps");
    g1->set(15.5);
    auto g2 = reg.getOrCreateGauge("bitrate_mbps");
    EXPECT_DOUBLE_EQ(g2->get(), 15.5);
}
