#include "mist/transport/qos/HostBandwidthBudgetManager.h"
#include "mist/common/Logger.h"
#include <algorithm>
#include <numeric>

namespace mist::transport::qos {

HostBandwidthBudgetManager::HostBandwidthBudgetManager(
    uint32_t uploadLimitKbps,
    uint32_t headroomPercent,
    uint32_t minGuestVideoKbps,
    uint32_t maxGuestVideoKbps
) noexcept
    : uploadLimitKbps_(uploadLimitKbps),
      headroomPercent_(headroomPercent),
      minGuestVideoKbps_(minGuestVideoKbps),
      maxGuestVideoKbps_(maxGuestVideoKbps) {
    (void)recalculateBudget();
}

void HostBandwidthBudgetManager::setUploadLimitKbps(uint32_t limitKbps) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    uploadLimitKbps_ = limitKbps;
    (void)recalculateBudget();
}

uint32_t HostBandwidthBudgetManager::getUploadLimitKbps() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return uploadLimitKbps_;
}

void HostBandwidthBudgetManager::setAllocationPolicy(AllocationPolicy policy) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    policy_ = policy;
    (void)recalculateBudget();
}

AllocationPolicy HostBandwidthBudgetManager::getAllocationPolicy() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return policy_;
}

common::Result<void> HostBandwidthBudgetManager::addGuest(
    const std::string& guestId,
    uint32_t requestedVideoKbps,
    bool isInteractive,
    uint32_t audioKbps,
    uint32_t controlKbps
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

    GuestDemand demand{};
    demand.guestId = guestId;
    demand.requestedVideoKbps = requestedVideoKbps;
    demand.isInteractive = isInteractive;
    demand.audioKbps = audioKbps;
    demand.controlKbps = controlKbps;
    demand.currentAllocation.guestId = guestId;
    demand.currentAllocation.reservedAudioKbps = audioKbps;
    demand.currentAllocation.reservedControlKbps = controlKbps;

    guests_.emplace(guestId, std::move(demand));
    (void)recalculateBudget();

    MIST_LOG_INFO(common::SubsystemId::Transport,
                  "HostBandwidthBudgetManager: Added guest " + guestId + " (Demand: " +
                  std::to_string(requestedVideoKbps) + " kbps)");
    return common::Result<void>::success();
}

common::Result<void> HostBandwidthBudgetManager::updateGuestDemand(
    const std::string& guestId,
    uint32_t requestedVideoKbps
) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it == guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound,
                             "Guest ID not found: " + guestId);
    }

    it->second.requestedVideoKbps = requestedVideoKbps;
    (void)recalculateBudget();
    return common::Result<void>::success();
}

common::Result<void> HostBandwidthBudgetManager::removeGuest(const std::string& guestId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it == guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound,
                             "Guest ID not found: " + guestId);
    }

    guests_.erase(it);
    (void)recalculateBudget();
    MIST_LOG_INFO(common::SubsystemId::Transport, "HostBandwidthBudgetManager: Removed guest " + guestId);
    return common::Result<void>::success();
}

bool HostBandwidthBudgetManager::hasGuest(const std::string& guestId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return guests_.find(guestId) != guests_.end();
}

size_t HostBandwidthBudgetManager::getGuestCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return guests_.size();
}

HostBudgetSnapshot HostBandwidthBudgetManager::recalculateBudget() {
    HostBudgetSnapshot snap{};
    snap.uploadLimitKbps = uploadLimitKbps_;
    snap.guestCount = guests_.size();

    // 1. Calculate reserved headroom to prevent bufferbloat
    snap.headroomKbps = (uploadLimitKbps_ * headroomPercent_) / 100;
    uint32_t baseAvailableKbps = (uploadLimitKbps_ > snap.headroomKbps)
        ? (uploadLimitKbps_ - snap.headroomKbps)
        : 0;

    // 2. Sum up mandatory reservations for audio and control channels
    for (const auto& [_, g] : guests_) {
        snap.totalAudioReservedKbps += g.audioKbps;
        snap.totalControlReservedKbps += g.controlKbps;
    }

    uint32_t totalReserved = snap.totalAudioReservedKbps + snap.totalControlReservedKbps;
    uint32_t availableForVideo = (baseAvailableKbps > totalReserved)
        ? (baseAvailableKbps - totalReserved)
        : 0;

    if (guests_.empty()) {
        snap.unallocatedKbps = baseAvailableKbps;
        lastSnapshot_ = snap;
        return snap;
    }

    // 3. Allocate available video bandwidth among guests
    size_t count = guests_.size();

    if (policy_ == AllocationPolicy::EqualShare) {
        uint32_t share = availableForVideo / static_cast<uint32_t>(count);
        for (auto& [_, g] : guests_) {
            uint32_t videoAlloc = std::min(g.requestedVideoKbps, share);
            videoAlloc = std::max(minGuestVideoKbps_, videoAlloc);
            videoAlloc = std::min(maxGuestVideoKbps_, videoAlloc);

            g.currentAllocation.allocatedVideoKbps = videoAlloc;
            g.currentAllocation.reservedAudioKbps = g.audioKbps;
            g.currentAllocation.reservedControlKbps = g.controlKbps;
            g.currentAllocation.totalGuestKbps = videoAlloc + g.audioKbps + g.controlKbps;
            g.currentAllocation.isThrottled = (videoAlloc < g.requestedVideoKbps);

            snap.totalVideoAllocatedKbps += videoAlloc;
            snap.guestAllocations.push_back(g.currentAllocation);
        }
    } else if (policy_ == AllocationPolicy::PriorityWeighted) {
        double totalWeight = 0.0;
        for (const auto& [_, g] : guests_) {
            totalWeight += (g.isInteractive ? 2.0 : 1.0);
        }

        for (auto& [_, g] : guests_) {
            double weight = (g.isInteractive ? 2.0 : 1.0);
            uint32_t weightedShare = static_cast<uint32_t>((static_cast<double>(availableForVideo) * weight) / totalWeight);
            uint32_t videoAlloc = std::min(g.requestedVideoKbps, weightedShare);
            videoAlloc = std::max(minGuestVideoKbps_, videoAlloc);
            videoAlloc = std::min(maxGuestVideoKbps_, videoAlloc);

            g.currentAllocation.allocatedVideoKbps = videoAlloc;
            g.currentAllocation.reservedAudioKbps = g.audioKbps;
            g.currentAllocation.reservedControlKbps = g.controlKbps;
            g.currentAllocation.totalGuestKbps = videoAlloc + g.audioKbps + g.controlKbps;
            g.currentAllocation.isThrottled = (videoAlloc < g.requestedVideoKbps);

            snap.totalVideoAllocatedKbps += videoAlloc;
            snap.guestAllocations.push_back(g.currentAllocation);
        }
    } else { // ProportionalDemand
        uint32_t totalDemand = 0;
        for (const auto& [_, g] : guests_) {
            totalDemand += g.requestedVideoKbps;
        }

        for (auto& [_, g] : guests_) {
            uint32_t videoAlloc = g.requestedVideoKbps;
            if (totalDemand > availableForVideo && totalDemand > 0) {
                videoAlloc = static_cast<uint32_t>((static_cast<uint64_t>(g.requestedVideoKbps) * availableForVideo) / totalDemand);
            }
            videoAlloc = std::max(minGuestVideoKbps_, videoAlloc);
            videoAlloc = std::min(maxGuestVideoKbps_, videoAlloc);

            g.currentAllocation.allocatedVideoKbps = videoAlloc;
            g.currentAllocation.reservedAudioKbps = g.audioKbps;
            g.currentAllocation.reservedControlKbps = g.controlKbps;
            g.currentAllocation.totalGuestKbps = videoAlloc + g.audioKbps + g.controlKbps;
            g.currentAllocation.isThrottled = (videoAlloc < g.requestedVideoKbps);

            snap.totalVideoAllocatedKbps += videoAlloc;
            snap.guestAllocations.push_back(g.currentAllocation);
        }
    }

    snap.totalAllocatedKbps = snap.totalVideoAllocatedKbps + snap.totalAudioReservedKbps + snap.totalControlReservedKbps;
    snap.unallocatedKbps = (baseAvailableKbps > snap.totalAllocatedKbps)
        ? (baseAvailableKbps - snap.totalAllocatedKbps)
        : 0;
    snap.isOversubscribed = (snap.totalAllocatedKbps + snap.headroomKbps > uploadLimitKbps_);

    lastSnapshot_ = snap;
    return snap;
}

common::Result<GuestAllocation> HostBandwidthBudgetManager::getGuestAllocation(const std::string& guestId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = guests_.find(guestId);
    if (it == guests_.end()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound,
                             "Guest ID not found: " + guestId);
    }
    return it->second.currentAllocation;
}

HostBudgetSnapshot HostBandwidthBudgetManager::getSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastSnapshot_;
}

} // namespace mist::transport::qos
