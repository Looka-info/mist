#include "mist/transport/qos/QualityAdaptationManager.h"
#include <algorithm>

namespace mist::transport::qos {

ProfileConfig QualityAdaptationManager::getStaticProfileConfig(QualityProfile profile) noexcept {
    ProfileConfig cfg{};
    cfg.profile = profile;
    switch (profile) {
        case QualityProfile::Performance:
            cfg.width = 1280;
            cfg.height = 720;
            cfg.targetFps = 60;
            cfg.minBitrateKbps = 1500;
            cfg.maxBitrateKbps = 6000;
            cfg.initialBitrateKbps = 3500;
            cfg.maxAcceptableRttMs = 45.0;
            cfg.lossThresholdPercent = 1.5;
            break;
        case QualityProfile::Balanced:
            cfg.width = 1920;
            cfg.height = 1080;
            cfg.targetFps = 60;
            cfg.minBitrateKbps = 3000;
            cfg.maxBitrateKbps = 14000;
            cfg.initialBitrateKbps = 8000;
            cfg.maxAcceptableRttMs = 60.0;
            cfg.lossThresholdPercent = 2.5;
            break;
        case QualityProfile::Quality:
            cfg.width = 1920;
            cfg.height = 1080;
            cfg.targetFps = 60;
            cfg.minBitrateKbps = 6000;
            cfg.maxBitrateKbps = 25000;
            cfg.initialBitrateKbps = 15000;
            cfg.maxAcceptableRttMs = 80.0;
            cfg.lossThresholdPercent = 3.5;
            break;
        case QualityProfile::Adaptive:
        default:
            cfg.width = 1920;
            cfg.height = 1080;
            cfg.targetFps = 60;
            cfg.minBitrateKbps = 1500;
            cfg.maxBitrateKbps = 25000;
            cfg.initialBitrateKbps = 8000;
            cfg.maxAcceptableRttMs = 65.0;
            cfg.lossThresholdPercent = 2.0;
            break;
    }
    return cfg;
}

QualityAdaptationManager::QualityAdaptationManager(QualityProfile initialProfile) noexcept
    : activeProfile_(initialProfile),
      config_(getStaticProfileConfig(initialProfile)) {}

void QualityAdaptationManager::setProfile(QualityProfile profile) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    activeProfile_ = profile;
    config_ = getStaticProfileConfig(profile);
}

QualityProfile QualityAdaptationManager::getProfile() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeProfile_;
}

ProfileConfig QualityAdaptationManager::getProfileConfig() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

BandwidthBudget QualityAdaptationManager::calculateBudget(uint32_t totalAvailableKbps) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    BandwidthBudget budget{};
    budget.totalBandwidthKbps = totalAvailableKbps;
    budget.audioBitrateKbps = 128;
    budget.controlBitrateKbps = 50;

    // Headroom: 5% of total or 150 kbps, whichever is higher
    budget.transportHeadroomKbps = std::max(150U, totalAvailableKbps / 20);

    uint32_t overhead = budget.audioBitrateKbps + budget.controlBitrateKbps + budget.transportHeadroomKbps;
    if (totalAvailableKbps > overhead) {
        budget.videoBitrateKbps = totalAvailableKbps - overhead;
    } else {
        budget.videoBitrateKbps = std::max(500U, totalAvailableKbps / 2);
    }

    return budget;
}

} // namespace mist::transport::qos
