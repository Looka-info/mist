#pragma once

#include <string>
#include <string_view>
#include <atomic>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace mist::telemetry {

class Counter {
public:
    explicit Counter(std::string name) : name_(std::move(name)), count_(0) {}

    void increment(uint64_t val = 1) noexcept { count_.fetch_add(val, std::memory_order_relaxed); }
    [[nodiscard]] uint64_t get() const noexcept { return count_.load(std::memory_order_relaxed); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    std::string name_;
    std::atomic<uint64_t> count_;
};

class Gauge {
public:
    explicit Gauge(std::string name) : name_(std::move(name)), value_(0.0) {}

    void set(double val) noexcept { value_.store(val, std::memory_order_relaxed); }
    [[nodiscard]] double get() const noexcept { return value_.load(std::memory_order_relaxed); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    std::string name_;
    std::atomic<double> value_;
};

struct HistogramSnapshot {
    uint64_t count{0};
    double minUs{0.0};
    double maxUs{0.0};
    double avgUs{0.0};
    double p50Us{0.0};
    double p95Us{0.0};
    double p99Us{0.0};
};

class LatencyHistogram {
public:
    explicit LatencyHistogram(std::string name, size_t maxSamples = 1000);

    void record(double microseconds);
    [[nodiscard]] HistogramSnapshot snapshot() const;
    void reset() noexcept;
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    std::string name_;
    size_t maxSamples_;
    mutable std::mutex mutex_;
    std::vector<double> samples_;
};

class MetricRegistry {
public:
    static MetricRegistry& instance() noexcept;

    std::shared_ptr<Counter> getOrCreateCounter(const std::string& name);
    std::shared_ptr<Gauge> getOrCreateGauge(const std::string& name);
    std::shared_ptr<LatencyHistogram> getOrCreateHistogram(const std::string& name);

    void resetAll();

private:
    MetricRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Counter>> counters_;
    std::unordered_map<std::string, std::shared_ptr<Gauge>> gauges_;
    std::unordered_map<std::string, std::shared_ptr<LatencyHistogram>> histograms_;
};

} // namespace mist::telemetry
