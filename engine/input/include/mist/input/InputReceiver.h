#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputProtocol.h"
#include "mist/input/InputPermissionManager.h"
#include "mist/input/InputDispatcher.h"
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

namespace mist::input {

class InputReceiver {
public:
    InputReceiver(
        std::shared_ptr<InputPermissionManager> permissionManager,
        std::shared_ptr<InputDispatcher> dispatcher
    ) noexcept;

    ~InputReceiver() = default;

    // Called when a packet arrives on a WebRTC DataChannel
    [[nodiscard]] common::Result<void> processPacket(
        const std::string& channelLabel,
        std::span<const uint8_t> data
    );

    void resetSequenceTracking() noexcept;
    [[nodiscard]] uint32_t getLastSequenceNumber() const noexcept { return lastSequenceNumber_.load(); }
    [[nodiscard]] uint64_t getPacketsReceived() const noexcept { return packetsReceived_.load(); }
    [[nodiscard]] uint64_t getUnauthorizedDropped() const noexcept { return unauthorizedDropped_.load(); }
    [[nodiscard]] uint64_t getStaleDropped() const noexcept { return staleDropped_.load(); }

private:
    std::shared_ptr<InputPermissionManager> permissionManager_;
    std::shared_ptr<InputDispatcher> dispatcher_;

    mutable std::mutex seqMutex_;
    std::atomic<uint32_t> lastSequenceNumber_{0};
    std::atomic<bool> hasReceivedFirstSequence_{false};

    std::atomic<uint64_t> packetsReceived_{0};
    std::atomic<uint64_t> unauthorizedDropped_{0};
    std::atomic<uint64_t> staleDropped_{0};
    std::atomic<uint64_t> rateLimitDropped_{0};
};

} // namespace mist::input