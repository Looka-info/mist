#include "mist/input/InputReceiver.h"
#include "mist/telemetry/Metrics.h"

namespace mist::input {

InputReceiver::InputReceiver(
    std::shared_ptr<InputPermissionManager> permissionManager,
    std::shared_ptr<InputDispatcher> dispatcher
) noexcept
    : permissionManager_(std::move(permissionManager)),
      dispatcher_(std::move(dispatcher)) {}

void InputReceiver::resetSequenceTracking() noexcept {
    lastSequenceNumber_.store(0);
    hasReceivedFirstSequence_.store(false);
}

common::Result<void> InputReceiver::processPacket(
    const std::string& /* channelLabel */,
    std::span<const uint8_t> data
) {
    packetsReceived_.fetch_add(1, std::memory_order_relaxed);
    telemetry::MetricRegistry::instance().getOrCreateCounter("input.packets_received")->increment();

    auto desResult = InputProtocol::deserialize(data);
    if (!desResult) {
        telemetry::MetricRegistry::instance().getOrCreateCounter("input.invalid_packets")->increment();
        return desResult.error();
    }

    const auto& [header, event] = desResult.value();
    auto type = static_cast<InputType>(header.inputType);

    // Rate limiting check
    if (permissionManager_ && !permissionManager_->checkRateLimit()) {
        rateLimitDropped_.fetch_add(1, std::memory_order_relaxed);
        telemetry::MetricRegistry::instance().getOrCreateCounter("input.rate_limited_events")->increment();
        return common::Error(common::SubsystemId::Input, common::ErrorCode::ResourceExhausted,
                             "Input event rate limit exceeded");
    }

    // Permission check
    std::string sessId(header.sessionId, strnlen(header.sessionId, sizeof(header.sessionId)));
    if (permissionManager_ && !permissionManager_->isInputAllowed(type, sessId)) {
        unauthorizedDropped_.fetch_add(1, std::memory_order_relaxed);
        telemetry::MetricRegistry::instance().getOrCreateCounter("input.unauthorized_events")->increment();
        return common::Error(common::SubsystemId::Input, common::ErrorCode::PermissionDenied,
                             "Remote input is not permitted or session unauthorized");
    }

    // Sequence number validation
    {
        std::lock_guard<std::mutex> lock(seqMutex_);
        if (hasReceivedFirstSequence_.load()) {
            uint32_t lastSeq = lastSequenceNumber_.load();
            if (header.sequenceNumber <= lastSeq && (lastSeq - header.sequenceNumber < 0x80000000u)) {
                // Out of order or duplicate
                staleDropped_.fetch_add(1, std::memory_order_relaxed);
                telemetry::MetricRegistry::instance().getOrCreateCounter("input.stale_events")->increment();
                // Reject stale state updates
                return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                                     "Stale or duplicate input sequence number");
            }
        }
        lastSequenceNumber_.store(header.sequenceNumber);
        hasReceivedFirstSequence_.store(true);
    }

    // Handle permission and recovery commands
    if (type == InputType::PermissionRequest) {
        if (permissionManager_ && std::holds_alternative<PermissionMessage>(event)) {
            permissionManager_->requestPermission(std::get<PermissionMessage>(event).requestedFeatures);
        }
        return common::Result<void>::success();
    }

    if (type == InputType::PermissionRevoke || type == InputType::ResetState) {
        if (permissionManager_) {
            permissionManager_->revokePermission();
        }
        if (dispatcher_) {
            (void)dispatcher_->reset();
        }
        return common::Result<void>::success();
    }

    // Dispatch to bounded queue
    if (dispatcher_) {
        return dispatcher_->enqueuePacket(header, event);
    }

    return common::Result<void>::success();
}

} // namespace mist::input