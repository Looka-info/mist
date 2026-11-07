#pragma once

#include "SessionState.h"
#include "ISession.h"
#include "mist/common/Result.h"
#include <mutex>
#include <vector>
#include <memory>

namespace mist::session {

class SessionStateMachine {
public:
    explicit SessionStateMachine(SessionState initialState = SessionState::Disconnected) noexcept;
    ~SessionStateMachine() = default;

    [[nodiscard]] SessionState getState() const noexcept;
    [[nodiscard]] common::Result<void> transitionTo(SessionState newState);

    void addObserver(std::weak_ptr<ISessionObserver> observer);
    void clearObservers() noexcept;

private:
    mutable std::mutex mutex_;
    SessionState currentState_;
    std::vector<std::weak_ptr<ISessionObserver>> observers_;
};

} // namespace mist::session
