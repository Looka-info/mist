#include "mist/transport/qos/PerGuestAdaptiveBitrate.h"
#include "mist/common/Logger.h"
#include <algorithm>

namespace mist::transport::qos {

PerGuestAdaptiveBitrateManager::PerGuestAdaptiveBitrateManager(uint32_t maxRenditions) noexcept
    : maxRenditions_(std::max(1u, maxRenditions)) {
    // Initialize default rendition slots
    renditions_.resize(maxRenditions_);
    for (uint32_t i = 0; i < maxRenditions_; ++i) {
        renditions_[i].renditionIndex = i;
        if (i == 0) {
            renditions_[i].name = "1080p60-primary";
            renditions_[i].width = 1920;
            renditions_[i].height = 1080;
            renditions_[i].targetFps = 60;
            renditions_[i].bitrateKbps = kDefaultMidBitrateKbps;
        } else {
            renditions_[i].name = "720p60-secondary";
            renditions_[i].width = 1280;
            renditions_[i].height = 720;
            renditions_[i].targetFps = 60;
            renditions_[i].bitrateKbps = kDefaultLowBitrateKbps;
        }
        renditions_[i].assignedGuestCount = 0;
    }
}

common::Result<void> PerGuestAdaptiveBitrateManager::registerGuest(
    const std::string& guestId,
    QualityProfile initialProfile
) {
    if (guestId.empty()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument,
                             "Guest ID cannot be empty");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (guests_.find(guestId) != guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::AlreadyExists,
                             "Guest ID already registered: " + guestId);
    }

    GuestContext ctx{};
    ctx.guestId = guestId;
    ctx.requestedProfile = initialProfile;
    ctx.controller = std::make_unique<AdaptiveBitrateController>(initialProfile);

    ctx.currentTarget.guestId = guestId;
    ctx.currentTarget.profile = initialProfile;
    ctx.currentTarget.width = 1920;
    ctx.currentTarget.height = 1080;
    ctx.currentTarget.targetFps = 60;
    ctx.currentTarget.targetBitrateKbps = ctx.controller->getCurrentVideoBitrateKbps();
    ctx.currentTarget.renditionIndex = 0;
    ctx.currentTarget.congestionState = CongestionState::Stable;
    ctx.currentTarget.profileReason = "Initial profile assigned";

    guests_.emplace(guestId, std::move(ctx));
    rebalanceRenditions();

    MIST_LOG_INFO(common::SubsystemId::Transport, "PerGuestAdaptiveBitrate: Registered guest " + guestId);
    return common::Result<void>::success();
}

common::Result<void> PerGuestAdaptiveBitrateManager::unregisterGuest(const std::string& guestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it == guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound,
                             "Guest ID not found: " + guestId);
    }

    guests_.erase(it);
    rebalanceRenditions();
    MIST_LOG_INFO(common::SubsystemId::Transport, "PerGuestAdaptiveBitrate: Unregistered guest " + guestId);
    return common::Result<void>::success();
}

bool PerGuestAdaptiveBitrateManager::hasGuest(const std::string& guestId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return guests_.find(guestId) != guests_.end();
}

size_t PerGuestAdaptiveBitrateManager::getGuestCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return guests_.size();
}

void PerGuestAdaptiveBitrateManager::onGuestRemb(const std::string& guestId, uint32_t bitrateBps, uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it != guests_.end()) {
        it->second.lastRembBps = bitrateBps;
        it->second.controller->onRembReceived(bitrateBps, nowMs);
    }
}

void PerGuestAdaptiveBitrateManager::onGuestRtcp(const std::string& guestId, const RtcpStatistics& stats, uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it != guests_.end()) {
        if (stats.receiverEstimatedMaxBitrateBps > 0) {
            it->second.lastRembBps = stats.receiverEstimatedMaxBitrateBps;
        }
        it->second.controller->onRtcpReportReceived(stats, nowMs);
        it->second.currentTarget.rttMs = stats.roundTripTimeMs;
        it->second.currentTarget.lossPercent = stats.lossPercent();
    }
}

void PerGuestAdaptiveBitrateManager::onGuestRtp(const std::string& guestId, const RtpStatistics& stats, uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it != guests_.end()) {
        it->second.controller->onRtpReportReceived(stats, nowMs);
    }
}

common::Result<bool> PerGuestAdaptiveBitrateManager::tick(uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool anyReconfigured = false;

    for (auto& [guestId, ctx] : guests_) {
        auto tickRes = ctx.controller->tick(nowMs);
        if (tickRes.hasValue() && tickRes.value()) {
            anyReconfigured = true;
        }

        ctx.currentTarget.targetBitrateKbps = ctx.controller->getCurrentVideoBitrateKbps();
        ctx.currentTarget.congestionState = ctx.controller->getCongestionState();

        uint32_t oldRendition = ctx.currentTarget.renditionIndex;
        uint32_t targetRendition = 0;

        // Determine resolution and tier based on network quality conditions
        if (ctx.currentTarget.lossPercent > 6.0 ||
            ctx.currentTarget.rttMs > 90.0 ||
            ctx.currentTarget.targetBitrateKbps < 5000) {
            // Constrained client: assign to 720p Performance rendition if multi-rendition enabled
            ctx.currentTarget.profile = QualityProfile::Performance;
            ctx.currentTarget.width = 1280;
            ctx.currentTarget.height = 720;
            ctx.currentTarget.targetFps = 60;
            ctx.currentTarget.profileReason = "High loss or high RTT detected -> 720p Performance";
            if (maxRenditions_ > 1) {
                targetRendition = 1;
            }
        } else if (ctx.currentTarget.lossPercent < 1.0 &&
                   ctx.currentTarget.rttMs < 35.0 &&
                   (ctx.lastRembBps >= 15000000 || ctx.currentTarget.targetBitrateKbps >= 15000 || ctx.requestedProfile == QualityProfile::Quality)) {
            // Excellent client: assign to 1080p Quality rendition
            ctx.currentTarget.profile = QualityProfile::Quality;
            ctx.currentTarget.width = 1920;
            ctx.currentTarget.height = 1080;
            ctx.currentTarget.targetFps = 60;
            ctx.currentTarget.profileReason = "Clean low-latency link with high bandwidth -> 1080p Quality";
            targetRendition = 0;
        } else {
            // Standard client: 1080p Balanced rendition
            ctx.currentTarget.profile = QualityProfile::Balanced;
            ctx.currentTarget.width = 1920;
            ctx.currentTarget.height = 1080;
            ctx.currentTarget.targetFps = 60;
            ctx.currentTarget.profileReason = "Nominal conditions -> 1080p Balanced";
            targetRendition = 0;
        }

        ctx.currentTarget.renditionIndex = targetRendition;

        if (oldRendition != targetRendition) {
            anyReconfigured = true;
            if (migrationCb_) {
                migrationCb_(guestId, oldRendition, targetRendition);
            }
        }
    }

    if (anyReconfigured) {
        rebalanceRenditions();
    }

    return anyReconfigured;
}

void PerGuestAdaptiveBitrateManager::rebalanceRenditions() {
    for (auto& r : renditions_) {
        r.assignedGuestCount = 0;
    }

    for (const auto& [_, ctx] : guests_) {
        uint32_t rIdx = ctx.currentTarget.renditionIndex;
        if (rIdx < renditions_.size()) {
            renditions_[rIdx].assignedGuestCount++;
        }
    }

    // Recompute bitrate per rendition based on assigned guests
    for (uint32_t i = 0; i < renditions_.size(); ++i) {
        uint32_t maxGuestBitrate = 0;
        uint32_t minGuestBitrate = UINT32_MAX;

        for (const auto& [_, ctx] : guests_) {
            if (ctx.currentTarget.renditionIndex == i) {
                maxGuestBitrate = std::max(maxGuestBitrate, ctx.currentTarget.targetBitrateKbps);
                minGuestBitrate = std::min(minGuestBitrate, ctx.currentTarget.targetBitrateKbps);
            }
        }

        if (renditions_[i].assignedGuestCount > 0) {
            // Target the conservative lower quartile of the rendition group to avoid packet loss
            renditions_[i].bitrateKbps = (minGuestBitrate != UINT32_MAX) ? minGuestBitrate : kDefaultMidBitrateKbps;
        } else {
            renditions_[i].bitrateKbps = (i == 0) ? kDefaultMidBitrateKbps : kDefaultLowBitrateKbps;
        }

        if (renditionCb_) {
            renditionCb_(i, renditions_[i].bitrateKbps, renditions_[i].width, renditions_[i].height);
        }
    }
}

common::Result<GuestRenditionTarget> PerGuestAdaptiveBitrateManager::getGuestTarget(const std::string& guestId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it == guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound,
                             "Guest ID not found: " + guestId);
    }
    return it->second.currentTarget;
}

std::vector<GuestRenditionTarget> PerGuestAdaptiveBitrateManager::getAllGuestTargets() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GuestRenditionTarget> targets;
    targets.reserve(guests_.size());
    for (const auto& [_, ctx] : guests_) {
        targets.push_back(ctx.currentTarget);
    }
    return targets;
}

std::vector<RenditionConfig> PerGuestAdaptiveBitrateManager::getActiveRenditions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return renditions_;
}

uint32_t PerGuestAdaptiveBitrateManager::getAggregateBitrateKbps() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t sum = 0;
    for (const auto& r : renditions_) {
        if (r.assignedGuestCount > 0) {
            sum += r.bitrateKbps;
        }
    }
    return (sum > 0) ? sum : renditions_[0].bitrateKbps;
}

void PerGuestAdaptiveBitrateManager::setRenditionReconfigCallback(RenditionReconfigCallback cb) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    renditionCb_ = std::move(cb);
}

void PerGuestAdaptiveBitrateManager::setGuestMigrationCallback(GuestMigrationCallback cb) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    migrationCb_ = std::move(cb);
}

} // namespace mist::transport::qos
