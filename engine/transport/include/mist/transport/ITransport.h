#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <vector>
#include <span>
#include <functional>

namespace mist::transport {

enum class TransportChannel : uint8_t {
    Video = 0,
    Audio,
    Input,
    Control
};

struct TransportConfig {
    std::string remoteAddress;
    uint16_t remotePort{0};
    uint16_t localPort{0};
    bool enableEncryption{true};
};

using DataReceivedCallback = std::function<void(TransportChannel channel, std::span<const uint8_t> data)>;

class ITransport {
public:
    virtual ~ITransport() = default;

    [[nodiscard]] virtual common::Result<void> initialize(const TransportConfig& config) = 0;
    [[nodiscard]] virtual common::Result<void> connect() = 0;
    [[nodiscard]] virtual common::Result<void> disconnect() = 0;
    [[nodiscard]] virtual common::Result<void> send(TransportChannel channel, std::span<const uint8_t> data) = 0;
    virtual void setDataReceivedCallback(DataReceivedCallback callback) = 0;
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
};

} // namespace mist::transport
