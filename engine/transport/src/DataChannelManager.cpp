#include "mist/transport/DataChannelManager.h"
#include "mist/common/Logger.h"
#include "rtc/rtc.hpp"

namespace mist::transport {

DataChannelManager::DataChannelManager() noexcept = default;

DataChannelManager::~DataChannelManager() {
    close();
}

common::Result<void> DataChannelManager::initialize(
    std::shared_ptr<rtc::PeerConnection> pc,
    bool isHost
) {
    std::lock_guard<std::mutex> lock(mutex_);
    isHost_ = isHost;

    if (!pc) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidArgument, "PeerConnection is null");
    }

    try {
        if (isHost_) {
            controlChannel_ = pc->createDataChannel("session-control");
            registerChannelCallbacks(controlChannel_);

            diagChannel_ = pc->createDataChannel("diagnostics");
            registerChannelCallbacks(diagChannel_);
        } else {
            pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
                std::lock_guard<std::mutex> innerLock(mutex_);
                std::string label = dc->label();
                MIST_LOG_INFO(common::SubsystemId::Transport, "Incoming remote DataChannel: " + label);
                if (label == "session-control") {
                    controlChannel_ = dc;
                    registerChannelCallbacks(controlChannel_);
                } else if (label == "diagnostics") {
                    diagChannel_ = dc;
                    registerChannelCallbacks(diagChannel_);
                } else {
                    registerChannelCallbacks(dc);
                }
            });
        }
        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Failed to initialize DataChannelManager: " + std::string(e.what()));
    }
}

void DataChannelManager::registerChannelCallbacks(std::shared_ptr<rtc::DataChannel> dc) {
    if (!dc) return;
    std::string label = dc->label();

    dc->onOpen([label]() {
        MIST_LOG_INFO(common::SubsystemId::Transport, "DataChannel open: " + label);
    });

    dc->onClosed([label]() {
        MIST_LOG_INFO(common::SubsystemId::Transport, "DataChannel closed: " + label);
    });

    dc->onError([label](std::string error) {
        MIST_LOG_ERROR(common::SubsystemId::Transport, "DataChannel [" + label + "] error: " + error);
    });

    dc->onMessage([this, label](rtc::message_variant data) {
        if (std::holds_alternative<rtc::binary>(data)) {
            const auto& bin = std::get<rtc::binary>(data);
            if (messageCb_) {
                messageCb_(label, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bin.data()), bin.size()));
            }
        } else if (std::holds_alternative<std::string>(data)) {
            const auto& str = std::get<std::string>(data);
            if (messageCb_) {
                messageCb_(label, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(str.data()), str.size()));
            }
        }
    });
}

common::Result<void> DataChannelManager::sendData(const std::string& label, std::span<const uint8_t> data) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<rtc::DataChannel> target = (label == "diagnostics") ? diagChannel_ : controlChannel_;
    if (!target || !target->isOpen()) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InvalidState, "DataChannel [" + label + "] is not open");
    }

    try {
        target->send(reinterpret_cast<const rtc::byte*>(data.data()), data.size());
        return common::Result<void>::success();
    } catch (const std::exception& e) {
        return common::Error(common::SubsystemId::Transport, common::ErrorCode::InternalError,
                             "Failed to send on DataChannel [" + label + "]: " + std::string(e.what()));
    }
}

common::Result<void> DataChannelManager::sendText(const std::string& label, const std::string& text) {
    return sendData(label, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

bool DataChannelManager::isChannelOpen(const std::string& label) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (label == "diagnostics") {
        return diagChannel_ && diagChannel_->isOpen();
    }
    return controlChannel_ && controlChannel_->isOpen();
}

void DataChannelManager::close() noexcept {
    std::shared_ptr<rtc::DataChannel> ctrl;
    std::shared_ptr<rtc::DataChannel> diag;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ctrl = std::move(controlChannel_);
        diag = std::move(diagChannel_);
    }
    if (ctrl) {
        try { ctrl->close(); } catch (...) {}
    }
    if (diag) {
        try { diag->close(); } catch (...) {}
    }
}

} // namespace mist::transport
