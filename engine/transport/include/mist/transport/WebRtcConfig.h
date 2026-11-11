#pragma once

#include "mist/transport/LowLatencyTuning.h"
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
        {"stun:stun1.l.google.com:19302", "", ""},
        {"stun:stun2.l.google.com:19302", "", ""},
        {"stun:stun3.l.google.com:19302", "", ""},
        {"turn:openrelay.metered.ca:80", "openrelay", "openrelay"},
        {"turn:openrelay.metered.ca:443", "openrelay", "openrelay"},
        {"turn:openrelay.metered.ca:443?transport=tcp", "openrelay", "openrelay"}
    };
    uint16_t minPort{0};
    uint16_t maxPort{0};
    uint32_t videoClockRate{90000};
    std::string videoCodec{"h264"}; // "h264" or "hevc"
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

    // Peer identity & security verification
    std::string expectedRemoteFingerprint;

    // WebRTC low-latency tuning & pacing
    LowLatencyProfile lowLatency{};
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
