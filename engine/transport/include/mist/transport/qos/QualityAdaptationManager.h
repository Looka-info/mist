#pragma once

#include "QosTypes.h"
#include <mutex>

namespace mist::transport::qos {

class QualityAdaptationManager {
public:
    explicit QualityAdaptationManager(QualityProfile initialProfile = QualityProfile::Adaptive) noexcept;
    ~QualityAdaptationManager() = default;

    void setProfile(QualityProfile profile) noexcept;
    [[nodiscard]] QualityProfile getProfile() const noexcept;
    [[nodiscard]] ProfileConfig getProfileConfig() const noexcept;

    [[nodiscard]] BandwidthBudget calculateBudget(uint32_t totalAvailableKbps) const noexcept;

    [[nodiscard]] static ProfileConfig getStaticProfileConfig(QualityProfile profile) noexcept;

private:
    mutable std::mutex mutex_;
    QualityProfile activeProfile_{QualityProfile::Adaptive};
    ProfileConfig config_;
};

} // namespace mist::transport::qos
