#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace mist::transport {

struct IceServer {
    std::string url;
    std::string username;
    std::string credential;
};

struct WebRtcTransportConfig {
    std::string sessionId{"mist-session-default"};
    std::string signalingUrl{"ws://127.0.0.1:8080"};
    std::string authToken;
    bool isHost{true};
    std::vector<IceServer> iceServers{
        {"stun:stun.l.google.com:19302", "", ""},
        {"stun:stun1.l.google.com:19302", "", ""}
    };
    uint16_t minPort{0};
    uint16_t maxPort{0};
    uint32_t videoClockRate{90000};
    uint32_t targetFps{60};
    uint32_t targetBitrateKbps{15000};
    bool enableDataChannels{true};
    bool enableAudio{false};
    uint32_t audioClockRate{48000};
    uint32_t audioBitrateKbps{128};

    // NAT traversal and ICE configuration
    bool enableIpv6{true};
    bool enableIceUdp{true};
    bool enableIceTcp{true};
    bool enablePortMapping{false}; // Requires explicit user consent for UPnP/NAT-PMP
    uint32_t portMappingLeaseSeconds{3600};
    uint32_t maxIceRestarts{3};
};

enum class WebRtcState {
    New,
    Connecting,
    Connected,
    Disconnected,
    Failed,
    Closed
};

} // namespace mist::transport
