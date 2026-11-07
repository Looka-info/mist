#pragma once

#include "SessionState.h"
#include "mist/common/Result.h"
#include <string>
#include <memory>
#include <functional>

namespace mist::session {

struct SessionInfo {
    std::string sessionId;
    std::string peerId;
    std::string clientAddress;
    uint16_t clientPort{0};
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t targetFps{60};
};

class ISessionObserver {
public:
    virtual ~ISessionObserver() = default;
    virtual void onStateChanged(SessionState oldState, SessionState newState) = 0;
    virtual void onError(const mist::common::Error& error) = 0;
};

class ISession {
public:
    virtual ~ISession() = default;

    [[nodiscard]] virtual common::Result<void> initialize() = 0;
    [[nodiscard]] virtual common::Result<void> connect() = 0;
    [[nodiscard]] virtual common::Result<void> startStreaming() = 0;
    [[nodiscard]] virtual common::Result<void> pauseStreaming() = 0;
    [[nodiscard]] virtual common::Result<void> resumeStreaming() = 0;
    [[nodiscard]] virtual common::Result<void> disconnect() = 0;

    [[nodiscard]] virtual SessionState getState() const noexcept = 0;
    [[nodiscard]] virtual const SessionInfo& getInfo() const noexcept = 0;
    virtual void setObserver(std::shared_ptr<ISessionObserver> observer) = 0;
};

} // namespace mist::session
