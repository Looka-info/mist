#include "mist/session/SessionStateMachine.h"
#include <sstream>

namespace mist::session {

SessionStateMachine::SessionStateMachine(SessionState initialState) noexcept
    : currentState_(initialState) {}

SessionState SessionStateMachine::getState() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentState_;
}

common::Result<void> SessionStateMachine::transitionTo(SessionState newState) {
    std::vector<std::shared_ptr<ISessionObserver>> validObservers;
    SessionState oldState;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        oldState = currentState_;

        if (oldState == newState) {
            return common::Result<void>::success();
        }

        if (!isValidTransition(oldState, newState)) {
            std::ostringstream oss;
            oss << "Invalid session state transition from "
                << sessionStateToString(oldState) << " to "
                << sessionStateToString(newState);
            return common::Error(common::SubsystemId::Session, common::ErrorCode::InvalidState, oss.str());
        }

        currentState_ = newState;

        // Collect living observers and purge expired ones
        auto it = observers_.begin();
        while (it != observers_.end()) {
            if (auto locked = it->lock()) {
                validObservers.push_back(locked);
                ++it;
            } else {
                it = observers_.erase(it);
            }
        }
    }

    // Notify observers outside the state machine lock to avoid deadlocks
    for (const auto& obs : validObservers) {
        obs->onStateChanged(oldState, newState);
    }

    return common::Result<void>::success();
}

void SessionStateMachine::addObserver(std::weak_ptr<ISessionObserver> observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.push_back(std::move(observer));
}

void SessionStateMachine::clearObservers() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.clear();
}

} // namespace mist::session
