#include "mist/transport/LowLatencyTuning.h"
#include <sstream>
#include <algorithm>

namespace mist::transport {

namespace {

constexpr const char* kPlayoutDelayUri = "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";

} // namespace

std::string LowLatencyTuner::injectPlayoutDelay(
    const std::string& sdp,
    uint32_t minFrames,
    uint32_t maxFrames,
    uint32_t targetFps
) noexcept {
    if (sdp.empty()) return sdp;

    uint32_t fps = (targetFps > 0) ? targetFps : 60;
    uint32_t minMs = (minFrames * 1000) / fps;
    uint32_t maxMs = (maxFrames * 1000) / fps;

    std::istringstream stream(sdp);
    std::string line;
    std::string result;
    result.reserve(sdp.size() + 256);

    bool inVideoSection = false;
    bool alreadyHasExtension = (sdp.find(kPlayoutDelayUri) != std::string::npos);
    bool injected = false;
    uint32_t maxExtId = 1;

    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);

        if (line.rfind("a=extmap:", 0) == 0) {
            size_t slash = line.find(' ');
            if (slash != std::string::npos) {
                try {
                    uint32_t id = static_cast<uint32_t>(std::stoul(line.substr(9, slash - 9)));
                    maxExtId = std::max(maxExtId, id);
                } catch (...) {}
            }
        }
    }

    uint32_t newExtId = maxExtId + 1;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& curLine = lines[i];
        result += curLine;
        result += "\r\n";

        if (curLine.rfind("m=video", 0) == 0) {
            inVideoSection = true;
        } else if (inVideoSection && curLine.rfind("m=", 0) == 0) {
            inVideoSection = false;
        }

        if (inVideoSection && !alreadyHasExtension && !injected) {
            // Inject extmap and playout delay attributes after m=video / c=IN lines
            if (curLine.rfind("c=IN", 0) == 0 || curLine.rfind("a=rtpmap", 0) == 0) {
                result += "a=extmap:" + std::to_string(newExtId) + " " + kPlayoutDelayUri + "\r\n";
                result += "a=playout-delay:" + std::to_string(minMs) + " " + std::to_string(maxMs) + "\r\n";
                injected = true;
            }
        }
    }

    if (!alreadyHasExtension && !injected) {
        // Fallback: append playout delay attribute to end of SDP
        result += "a=extmap:" + std::to_string(newExtId) + " " + kPlayoutDelayUri + "\r\n";
        result += "a=playout-delay:" + std::to_string(minMs) + " " + std::to_string(maxMs) + "\r\n";
    }

    return result;
}

std::pair<uint32_t, uint32_t> LowLatencyTuner::parsePlayoutDelayMs(
    const std::string& sdp
) noexcept {
    if (sdp.empty()) {
        return {0, 16}; // Default 0-1 frame @ 60fps
    }

    std::istringstream stream(sdp);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("a=playout-delay:", 0) == 0) {
            std::string vals = line.substr(16);
            size_t spacePos = vals.find(' ');
            if (spacePos != std::string::npos) {
                try {
                    uint32_t minMs = static_cast<uint32_t>(std::stoul(vals.substr(0, spacePos)));
                    uint32_t maxMs = static_cast<uint32_t>(std::stoul(vals.substr(spacePos + 1)));
                    return {minMs, maxMs};
                } catch (...) {}
            }
        }
    }

    return {0, 16};
}

uint32_t LowLatencyTuner::calculateOptimalRtcpIntervalMs(
    uint32_t rttMs,
    uint32_t targetFps,
    uint32_t guestCount
) noexcept {
    (void)targetFps;
    // Base interval on network latency: faster feedback on low RTT
    uint32_t baseIntervalMs = 50;
    if (rttMs > 60) {
        baseIntervalMs = 100;
    } else if (rttMs > 30) {
        baseIntervalMs = 75;
    }

    // Add mild scaling per connected guest to avoid RTCP bandwidth explosion
    uint32_t guestOffset = (guestCount > 1) ? (guestCount - 1) * 5 : 0;
    uint32_t interval = baseIntervalMs + guestOffset;

    return std::clamp(interval, 40u, 250u);
}

} // namespace mist::transport
