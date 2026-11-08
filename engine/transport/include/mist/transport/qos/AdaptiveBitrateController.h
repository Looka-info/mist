#pragma once

#include "QosTypes.h"
#include "NetworkQualityEstimator.h"
#include "CongestionController.h"
#include "QualityAdaptationManager.h"
#include "mist/common/Result.h"
#include <functional>
#include <memory>
#include <mutex>

namespace mist::transport::qos {

class AdaptiveBitrateController {
public:
    using EncoderReconfigCallback = std::function<common::Result<void>(uint32_t newBitrateKbps)>;

    explicit AdaptiveBitrateController(QualityProfile profile = QualityProfile::Adaptive) noexcept;
    ~AdaptiveBitrateController() = default;

    void setEncoderReconfigCallback(EncoderReconfigCallback cb) noexcept;
    void setQualityProfile(QualityProfile profile) noexcept;
    [[nodiscard]] QualityProfile getQualityProfile() const noexcept;

    void onRembReceived(uint32_t bitrateBps, uint64_t nowMs = 0);
    void onRtcpReportReceived(const RtcpStatistics& stats, uint64_t nowMs = 0);
    void onRtpReportReceived(const RtpStatistics& stats, uint64_t nowMs = 0);

    // Periodic evaluation loop (e.g. called every 250ms - 500ms)
    [[nodiscard]] common::Result<bool> tick(uint64_t nowMs = 0);

    [[nodiscard]] CongestionState getCongestionState() const noexcept;
    [[nodiscard]] uint32_t getCurrentVideoBitrateKbps() const noexcept;
    [[nodiscard]] BandwidthBudget getCurrentBudget() const noexcept;
    [[nodiscard]] NetworkQualityEstimator& getEstimator() noexcept { return estimator_; }
    [[nodiscard]] const NetworkQualityEstimator& getEstimator() const noexcept { return estimator_; }

    [[nodiscard]] uint64_t getReconfigurationCount() const noexcept { return reconfigCount_; }

private:
    [[nodiscard]] uint64_t getNowMs() const noexcept;

    mutable std::mutex mutex_;
    NetworkQualityEstimator estimator_;
    QualityAdaptationManager qualityManager_;
    CongestionController congestionController_;

    EncoderReconfigCallback encoderCb_;
    uint32_t lastAppliedVideoBitrateKbps_{0};
    uint64_t lastTickMs_{0};
    uint64_t reconfigCount_{0};

    static constexpr uint32_t kMinReconfigDeltaKbps = 250;
    static constexpr uint64_t kDefaultTickIntervalMs = 500;
};

} // namespace mist::transport::qos
