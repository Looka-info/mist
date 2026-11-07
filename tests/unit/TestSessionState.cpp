#include <gtest/gtest.h>
#include "mist/session/SessionState.h"
#include "mist/session/SessionStateMachine.h"

using namespace mist::session;
using namespace mist::common;

class MockSessionObserver : public ISessionObserver {
public:
    void onStateChanged(SessionState oldState, SessionState newState) override {
        transitions.emplace_back(oldState, newState);
    }
    void onError(const Error& err) override {
        errors.push_back(err);
    }

    std::vector<std::pair<SessionState, SessionState>> transitions;
    std::vector<Error> errors;
};

TEST(SessionStateTest, StateTransitions) {
    EXPECT_TRUE(isValidTransition(SessionState::Disconnected, SessionState::Initializing));
    EXPECT_TRUE(isValidTransition(SessionState::Initializing, SessionState::Connecting));
    EXPECT_TRUE(isValidTransition(SessionState::Connecting, SessionState::Connected));
    EXPECT_TRUE(isValidTransition(SessionState::Connected, SessionState::Streaming));
    EXPECT_TRUE(isValidTransition(SessionState::Streaming, SessionState::Paused));
    EXPECT_TRUE(isValidTransition(SessionState::Paused, SessionState::Streaming));
    EXPECT_TRUE(isValidTransition(SessionState::Streaming, SessionState::Disconnecting));
    EXPECT_TRUE(isValidTransition(SessionState::Disconnecting, SessionState::Disconnected));

    // Invalid transitions
    EXPECT_FALSE(isValidTransition(SessionState::Disconnected, SessionState::Streaming));
    EXPECT_FALSE(isValidTransition(SessionState::Disconnected, SessionState::Paused));
    EXPECT_FALSE(isValidTransition(SessionState::Streaming, SessionState::Initializing));
}

TEST(SessionStateTest, StateMachineTransitionExecution) {
    SessionStateMachine sm(SessionState::Disconnected);
    EXPECT_EQ(sm.getState(), SessionState::Disconnected);

    auto obs = std::make_shared<MockSessionObserver>();
    sm.addObserver(obs);

    auto res = sm.transitionTo(SessionState::Initializing);
    EXPECT_TRUE(res.hasValue());
    EXPECT_EQ(sm.getState(), SessionState::Initializing);

    res = sm.transitionTo(SessionState::Connecting);
    EXPECT_TRUE(res.hasValue());
    EXPECT_EQ(sm.getState(), SessionState::Connecting);

    res = sm.transitionTo(SessionState::Connected);
    EXPECT_TRUE(res.hasValue());
    EXPECT_EQ(sm.getState(), SessionState::Connected);

    ASSERT_EQ(obs->transitions.size(), 3u);
    EXPECT_EQ(obs->transitions[0], std::make_pair(SessionState::Disconnected, SessionState::Initializing));
    EXPECT_EQ(obs->transitions[1], std::make_pair(SessionState::Initializing, SessionState::Connecting));
    EXPECT_EQ(obs->transitions[2], std::make_pair(SessionState::Connecting, SessionState::Connected));

    // Attempt invalid transition: Connected -> Initializing
    auto invalidRes = sm.transitionTo(SessionState::Initializing);
    EXPECT_FALSE(invalidRes.hasValue());
    EXPECT_EQ(invalidRes.error().code(), ErrorCode::InvalidState);
    EXPECT_EQ(sm.getState(), SessionState::Connected);
    EXPECT_EQ(obs->transitions.size(), 3u); // Observer not notified on rejected transition
}
