#include "mist/transport/qos/AdaptiveBitrateController.h"
#include "mist/common/Logger.h"
#include <chrono>
#include <cmath>

namespace mist::transport::qos {

AdaptiveBitrateController::AdaptiveBitrateController(QualityProfile profile) noexcept
    : qualityManager_(profile),
      congestionController_(qualityManager_.getProfileConfig()) {
    lastAppliedVideoBitrateKbps_ = qualityManager_.getProfileConfig().initialBitrateKbps;
}

uint64_t AdaptiveBitrateController::getNowMs() const noexcept {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void AdaptiveBitrateController::setEncoderReconfigCallback(EncoderReconfigCallback cb) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    encoderCb_ = std::move(cb);
}

void AdaptiveBitrateController::setQualityProfile(QualityProfile profile) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    qualityManager_.setProfile(profile);
    ProfileConfig cfg = qualityManager_.getProfileConfig();
    congestionController_.updateProfileConfig(cfg);
}

QualityProfile AdaptiveBitrateController::getQualityProfile() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return qualityManager_.getProfile();
}

void AdaptiveBitrateController::onRembReceived(uint32_t bitrateBps, uint64_t nowMs) {
    if (nowMs == 0) nowMs = getNowMs();
    estimator_.updateRembBitrate(bitrateBps);
    (void)tick(nowMs);
}

void AdaptiveBitrateController::onRtcpReportReceived(const RtcpStatistics& stats, uint64_t nowMs) {
    if (nowMs == 0) nowMs = getNowMs();
    estimator_.updateRtcpStats(stats);
    (void)tick(nowMs);
}

void AdaptiveBitrateController::onRtpReportReceived(const RtpStatistics& stats, uint64_t nowMs) {
    if (nowMs == 0) nowMs = getNowMs();
    estimator_.updateRtpStats(stats);
}

common::Result<bool> AdaptiveBitrateController::tick(uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nowMs == 0) nowMs = getNowMs();

    // Rate-limit evaluation ticks to avoid excessive churning
    if (lastTickMs_ > 0 && (nowMs - lastTickMs_ < (kDefaultTickIntervalMs / 2))) {
        return false;
    }
    lastTickMs_ = nowMs;

    double rtt = estimator_.getSmoothedRttMs();
    double loss = estimator_.getSmoothedLossPercent();
    double jitter = estimator_.getSmoothedJitterMs();
    uint32_t estBwBps = estimator_.getEstimatedBandwidthBps();

    // 1. Update congestion controller
    uint32_t targetTotalKbps = congestionController_.processFeedback(rtt, loss, jitter, estBwBps, nowMs);

    // 2. Compute bandwidth budget
    BandwidthBudget budget = qualityManager_.calculateBudget(targetTotalKbps);
    uint32_t targetVideoKbps = budget.videoBitrateKbps;

    // 3. Determine if reconfiguration is needed
    uint32_t delta = (targetVideoKbps > lastAppliedVideoBitrateKbps_)
        ? (targetVideoKbps - lastAppliedVideoBitrateKbps_)
        : (lastAppliedVideoBitrateKbps_ - targetVideoKbps);

    CongestionState state = congestionController_.getState();
    bool isUrgentState = (state == CongestionState::Congested || state == CongestionState::CriticallyCongested);

    if (delta >= kMinReconfigDeltaKbps || (isUrgentState && targetVideoKbps < lastAppliedVideoBitrateKbps_)) {
        if (encoderCb_) {
            auto res = encoderCb_(targetVideoKbps);
            if (!res) {
                MIST_LOG_WARN(common::SubsystemId::Transport, "Encoder bitrate reconfiguration failed: " + res.error().message());
                return res.error();
            }
        }
        MIST_LOG_INFO(common::SubsystemId::Transport,
                      "Adaptive video bitrate updated: " + std::to_string(lastAppliedVideoBitrateKbps_) +
                      " kbps -> " + std::to_string(targetVideoKbps) + " kbps [State: " +
                      std::string(congestionStateToString(state)) + ", RTT: " + std::to_string(rtt) +
                      " ms, Loss: " + std::to_string(loss) + "%]");
        lastAppliedVideoBitrateKbps_ = targetVideoKbps;
        reconfigCount_++;
        return true;
    }

    return false;
}

CongestionState AdaptiveBitrateController::getCongestionState() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return congestionController_.getState();
}

uint32_t AdaptiveBitrateController::getCurrentVideoBitrateKbps() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastAppliedVideoBitrateKbps_;
}

BandwidthBudget AdaptiveBitrateController::getCurrentBudget() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return qualityManager_.calculateBudget(congestionController_.getCurrentBitrateKbps());
}

} // namespace mist::transport::qos
