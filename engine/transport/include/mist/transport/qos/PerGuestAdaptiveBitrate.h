#pragma once

#include "QosTypes.h"
#include "AdaptiveBitrateController.h"
#include "mist/common/Result.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>

namespace mist::transport::qos {

struct GuestRenditionTarget {
    std::string guestId;
    QualityProfile profile{QualityProfile::Adaptive};
    uint32_t targetBitrateKbps{8000};
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t targetFps{60};
    uint32_t renditionIndex{0};
    CongestionState congestionState{CongestionState::Stable};
    double rttMs{0.0};
    double lossPercent{0.0};
    std::string profileReason;
};

struct RenditionConfig {
    uint32_t renditionIndex{0};
    std::string name;
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t targetFps{60};
    uint32_t bitrateKbps{10000};
    size_t assignedGuestCount{0};
};

using RenditionReconfigCallback = std::function<void(uint32_t renditionIndex, uint32_t bitrateKbps, uint32_t width, uint32_t height)>;
using GuestMigrationCallback = std::function<void(const std::string& guestId, uint32_t oldRendition, uint32_t newRendition)>;

class PerGuestAdaptiveBitrateManager {
public:
    explicit PerGuestAdaptiveBitrateManager(uint32_t maxRenditions = 2) noexcept;
    ~PerGuestAdaptiveBitrateManager() = default;

    [[nodiscard]] common::Result<void> registerGuest(
        const std::string& guestId,
        QualityProfile initialProfile = QualityProfile::Adaptive
    );

    [[nodiscard]] common::Result<void> unregisterGuest(const std::string& guestId);
    [[nodiscard]] bool hasGuest(const std::string& guestId) const noexcept;
    [[nodiscard]] size_t getGuestCount() const noexcept;

    // Ingest telemetry per guest
    void onGuestRemb(const std::string& guestId, uint32_t bitrateBps, uint64_t nowMs = 0);
    void onGuestRtcp(const std::string& guestId, const RtcpStatistics& stats, uint64_t nowMs = 0);
    void onGuestRtp(const std::string& guestId, const RtpStatistics& stats, uint64_t nowMs = 0);

    // Periodic evaluation loop
    [[nodiscard]] common::Result<bool> tick(uint64_t nowMs = 0);

    // Rendition & QoS query
    [[nodiscard]] common::Result<GuestRenditionTarget> getGuestTarget(const std::string& guestId) const;
    [[nodiscard]] std::vector<GuestRenditionTarget> getAllGuestTargets() const;
    [[nodiscard]] std::vector<RenditionConfig> getActiveRenditions() const;
    [[nodiscard]] uint32_t getAggregateBitrateKbps() const noexcept;

    // Callbacks
    void setRenditionReconfigCallback(RenditionReconfigCallback cb) noexcept;
    void setGuestMigrationCallback(GuestMigrationCallback cb) noexcept;

private:
    struct GuestContext {
        std::string guestId;
        QualityProfile requestedProfile{QualityProfile::Adaptive};
        std::unique_ptr<AdaptiveBitrateController> controller;
        GuestRenditionTarget currentTarget;
        uint32_t lastRembBps{0};
        uint64_t lastEvaluatedMs{0};
    };

    void rebalanceRenditions();

    mutable std::mutex mutex_;
    uint32_t maxRenditions_{2};
    std::unordered_map<std::string, GuestContext> guests_;
    std::vector<RenditionConfig> renditions_;

    RenditionReconfigCallback renditionCb_;
    GuestMigrationCallback migrationCb_;

    static constexpr uint32_t kDefaultHighBitrateKbps = 18000;
    static constexpr uint32_t kDefaultMidBitrateKbps = 10000;
    static constexpr uint32_t kDefaultLowBitrateKbps = 4000;
};

} // namespace mist::transport::qos
