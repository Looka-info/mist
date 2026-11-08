#pragma once

#include "mist/common/Result.h"
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

namespace rtc {
class PeerConnection;
class DataChannel;
}

namespace mist::transport {

using DataChannelMessageCallback = std::function<void(const std::string& label, std::span<const uint8_t> data)>;

class DataChannelManager {
public:
    DataChannelManager() noexcept;
    ~DataChannelManager();

    [[nodiscard]] common::Result<void> initialize(
        std::shared_ptr<rtc::PeerConnection> pc,
        bool isHost
    );

    [[nodiscard]] common::Result<void> sendData(const std::string& label, std::span<const uint8_t> data);
    [[nodiscard]] common::Result<void> sendText(const std::string& label, const std::string& text);

    void setMessageCallback(DataChannelMessageCallback cb) { messageCb_ = std::move(cb); }
    [[nodiscard]] bool isChannelOpen(const std::string& label) const noexcept;

    void close() noexcept;

private:
    void registerChannelCallbacks(std::shared_ptr<rtc::DataChannel> dc);

    mutable std::mutex mutex_;
    bool isHost_{true};
    std::shared_ptr<rtc::DataChannel> controlChannel_;
    std::shared_ptr<rtc::DataChannel> diagChannel_;
    DataChannelMessageCallback messageCb_;
};

} // namespace mist::transport
