#pragma once

#include "QosTypes.h"
#include "mist/common/Result.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace mist::transport::qos {

enum class AllocationPolicy : uint8_t {
    EqualShare = 0,     // Fair equal division of available video bandwidth
    PriorityWeighted,   // Interactive guests get higher priority weight
    ProportionalDemand  // Distributed proportionally to requested capacity
};

struct GuestAllocation {
    std::string guestId;
    uint32_t allocatedVideoKbps{0};
    uint32_t reservedAudioKbps{128};
    uint32_t reservedControlKbps{64};
    uint32_t totalGuestKbps{0};
    bool isThrottled{false};
};

struct HostBudgetSnapshot {
    uint32_t uploadLimitKbps{30000};
    uint32_t totalAllocatedKbps{0};
    uint32_t totalVideoAllocatedKbps{0};
    uint32_t totalAudioReservedKbps{0};
    uint32_t totalControlReservedKbps{0};
    uint32_t headroomKbps{0};
    uint32_t unallocatedKbps{0};
    size_t guestCount{0};
    bool isOversubscribed{false};
    std::vector<GuestAllocation> guestAllocations;
};

class HostBandwidthBudgetManager {
public:
    explicit HostBandwidthBudgetManager(
        uint32_t uploadLimitKbps = 30000,
        uint32_t headroomPercent = 5,
        uint32_t minGuestVideoKbps = 2000,
        uint32_t maxGuestVideoKbps = 25000
    ) noexcept;

    ~HostBandwidthBudgetManager() = default;

    void setUploadLimitKbps(uint32_t limitKbps) noexcept;
    [[nodiscard]] uint32_t getUploadLimitKbps() const noexcept;

    void setAllocationPolicy(AllocationPolicy policy) noexcept;
    [[nodiscard]] AllocationPolicy getAllocationPolicy() const noexcept;

    [[nodiscard]] common::Result<void> addGuest(
        const std::string& guestId,
        uint32_t requestedVideoKbps = 10000,
        bool isInteractive = true,
        uint32_t audioKbps = 128,
        uint32_t controlKbps = 64
    );

    [[nodiscard]] common::Result<void> updateGuestDemand(
        const std::string& guestId,
        uint32_t requestedVideoKbps
    );

    [[nodiscard]] common::Result<void> removeGuest(const std::string& guestId);
    [[nodiscard]] bool hasGuest(const std::string& guestId) const noexcept;
    [[nodiscard]] size_t getGuestCount() const noexcept;

    // Recalculates allocations and returns the complete budget snapshot
    [[nodiscard]] HostBudgetSnapshot recalculateBudget();

    [[nodiscard]] common::Result<GuestAllocation> getGuestAllocation(const std::string& guestId) const;
    [[nodiscard]] HostBudgetSnapshot getSnapshot() const;

private:
    struct GuestDemand {
        std::string guestId;
        uint32_t requestedVideoKbps{10000};
        bool isInteractive{true};
        uint32_t audioKbps{128};
        uint32_t controlKbps{64};
        GuestAllocation currentAllocation;
    };

    mutable std::mutex mutex_;
    uint32_t uploadLimitKbps_{30000};
    uint32_t headroomPercent_{5};
    uint32_t minGuestVideoKbps_{2000};
    uint32_t maxGuestVideoKbps_{25000};
    AllocationPolicy policy_{AllocationPolicy::EqualShare};

    std::unordered_map<std::string, GuestDemand> guests_;
    HostBudgetSnapshot lastSnapshot_;
};

} // namespace mist::transport::qos
