#include "mist/telemetry/Metrics.h"
#include <algorithm>
#include <numeric>

namespace mist::telemetry {

LatencyHistogram::LatencyHistogram(std::string name, size_t maxSamples)
    : name_(std::move(name)), maxSamples_(maxSamples) {
    samples_.reserve(maxSamples_);
}

void LatencyHistogram::record(double microseconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.size() >= maxSamples_) {
        // shortcut: simple ring overwrite to keep bounded memory without heavy queue overhead
        samples_.erase(samples_.begin());
    }
    samples_.push_back(microseconds);
}

HistogramSnapshot LatencyHistogram::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.empty()) {
        return HistogramSnapshot{};
    }

    std::vector<double> sorted = samples_;
    std::sort(sorted.begin(), sorted.end());

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    double avg = sum / static_cast<double>(sorted.size());

    auto percentile = [&sorted](double p) -> double {
        if (sorted.empty()) return 0.0;
        size_t idx = static_cast<size_t>(p * static_cast<double>(sorted.size() - 1));
        return sorted[idx];
    };

    return HistogramSnapshot{
        .count = static_cast<uint64_t>(sorted.size()),
        .minUs = sorted.front(),
        .maxUs = sorted.back(),
        .avgUs = avg,
        .p50Us = percentile(0.50),
        .p95Us = percentile(0.95),
        .p99Us = percentile(0.99)
    };
}

void LatencyHistogram::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    samples_.clear();
}

MetricRegistry& MetricRegistry::instance() noexcept {
    static MetricRegistry reg;
    return reg;
}

std::shared_ptr<Counter> MetricRegistry::getOrCreateCounter(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = counters_.find(name);
    if (it != counters_.end()) return it->second;
    auto counter = std::make_shared<Counter>(name);
    counters_[name] = counter;
    return counter;
}

std::shared_ptr<Gauge> MetricRegistry::getOrCreateGauge(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gauges_.find(name);
    if (it != gauges_.end()) return it->second;
    auto gauge = std::make_shared<Gauge>(name);
    gauges_[name] = gauge;
    return gauge;
}

std::shared_ptr<LatencyHistogram> MetricRegistry::getOrCreateHistogram(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = histograms_.find(name);
    if (it != histograms_.end()) return it->second;
    auto hist = std::make_shared<LatencyHistogram>(name);
    histograms_[name] = hist;
    return hist;
}

void MetricRegistry::resetAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_.clear();
    gauges_.clear();
    histograms_.clear();
}

} // namespace mist::telemetry
