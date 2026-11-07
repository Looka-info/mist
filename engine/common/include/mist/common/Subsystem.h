#pragma once

#include <cstdint>
#include <string_view>

namespace mist::common {

enum class SubsystemId : uint8_t {
    Common,
    Platform,
    Session,
    Capture,
    Encode,
    Decode,
    Render,
    Transport,
    Audio,
    Input,
    Telemetry,
    Security,
    HostApp,
    ClientApp,
    DesktopApp,
    Signaling
};

[[nodiscard]] constexpr std::string_view subsystemToString(SubsystemId id) noexcept {
    switch (id) {
        case SubsystemId::Common:     return "Common";
        case SubsystemId::Platform:   return "Platform";
        case SubsystemId::Session:    return "Session";
        case SubsystemId::Capture:    return "Capture";
        case SubsystemId::Encode:     return "Encode";
        case SubsystemId::Decode:     return "Decode";
        case SubsystemId::Render:     return "Render";
        case SubsystemId::Transport:  return "Transport";
        case SubsystemId::Audio:      return "Audio";
        case SubsystemId::Input:      return "Input";
        case SubsystemId::Telemetry:  return "Telemetry";
        case SubsystemId::Security:   return "Security";
        case SubsystemId::HostApp:    return "HostApp";
        case SubsystemId::ClientApp:  return "ClientApp";
        case SubsystemId::DesktopApp: return "DesktopApp";
        case SubsystemId::Signaling:  return "Signaling";
    }
    return "Unknown";
}

} // namespace mist::common
