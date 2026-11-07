#pragma once

#include <cstdint>
#include <string_view>

namespace mist::session {

enum class SessionState : uint8_t {
    Disconnected = 0,
    Initializing,
    Connecting,
    Connected,
    Streaming,
    Paused,
    Disconnecting,
    Failed
};

[[nodiscard]] constexpr std::string_view sessionStateToString(SessionState state) noexcept {
    switch (state) {
        case SessionState::Disconnected:  return "Disconnected";
        case SessionState::Initializing:  return "Initializing";
        case SessionState::Connecting:    return "Connecting";
        case SessionState::Connected:     return "Connected";
        case SessionState::Streaming:     return "Streaming";
        case SessionState::Paused:        return "Paused";
        case SessionState::Disconnecting: return "Disconnecting";
        case SessionState::Failed:        return "Failed";
    }
    return "UnknownState";
}

[[nodiscard]] constexpr bool isValidTransition(SessionState from, SessionState to) noexcept {
    if (from == to) return true; // Re-entry to same state is a no-op

    switch (from) {
        case SessionState::Disconnected:
            return to == SessionState::Initializing || to == SessionState::Connecting;

        case SessionState::Initializing:
            return to == SessionState::Connecting || to == SessionState::Failed || to == SessionState::Disconnected;

        case SessionState::Connecting:
            return to == SessionState::Connected || to == SessionState::Failed || to == SessionState::Disconnecting;

        case SessionState::Connected:
            return to == SessionState::Streaming || to == SessionState::Disconnecting || to == SessionState::Failed;

        case SessionState::Streaming:
            return to == SessionState::Paused || to == SessionState::Disconnecting || to == SessionState::Failed;

        case SessionState::Paused:
            return to == SessionState::Streaming || to == SessionState::Disconnecting || to == SessionState::Failed;

        case SessionState::Disconnecting:
            return to == SessionState::Disconnected || to == SessionState::Failed;

        case SessionState::Failed:
            return to == SessionState::Disconnected || to == SessionState::Initializing;
    }
    return false;
}

} // namespace mist::session
