#include "mist/transport/NatTraversal.h"
#include "mist/common/Logger.h"
#include <sstream>
#include <cstring>
#include <algorithm>

namespace mist::transport {

uint32_t IceCandidatePrioritizer::computeRfc8445Priority(CandidateType type, AddressFamily family, uint32_t component) noexcept {
    uint32_t typePref = 0;
    switch (type) {
        case CandidateType::Host:            typePref = 126; break;
        case CandidateType::PeerReflexive:   typePref = 110; break;
        case CandidateType::ServerReflexive: typePref = 100; break;
        case CandidateType::Relayed:         typePref = 0;   break;
        default:                             typePref = 0;   break;
    }

    // RFC 8445 / RFC 6724: IPv6 has higher local preference (65535) over IPv4 (65534)
    uint32_t localPref = (family == AddressFamily::IPv6) ? 65535 : 65534;
    uint32_t compId = (component >= 1 && component <= 256) ? component : 1;

    return (typePref << 24) | (localPref << 8) | (256 - compId);
}

AddressFamily IceCandidatePrioritizer::determineAddressFamily(const std::string& ip) noexcept {
    if (ip.find(':') != std::string::npos) {
        return AddressFamily::IPv6;
    }
    if (ip.find('.') != std::string::npos) {
        return AddressFamily::IPv4;
    }
    return AddressFamily::Unknown;
}

bool IceCandidatePrioritizer::isLoopbackAddress(const std::string& ip) noexcept {
    if (ip == "127.0.0.1" || ip == "::1" || ip.starts_with("127.")) {
        return true;
    }
    return false;
}

bool IceCandidatePrioritizer::isCgnatAddress(const std::string& ip) noexcept {
    // RFC 6598: 100.64.0.0/10 (100.64.0.0 to 100.127.255.255)
    if (!ip.starts_with("100.")) return false;

    int oct1 = 0, oct2 = 0, oct3 = 0, oct4 = 0;
    if (sscanf_s(ip.c_str(), "%d.%d.%d.%d", &oct1, &oct2, &oct3, &oct4) == 4) {
        return (oct1 == 100 && oct2 >= 64 && oct2 <= 127);
    }
    return false;
}

bool IceCandidatePrioritizer::isPrivateAddress(const std::string& ip) noexcept {
    if (isLoopbackAddress(ip)) return true;

    if (ip.find(':') != std::string::npos) {
        // IPv6 Unique Local (fc00::/7) or Link-Local (fe80::/10)
        return ip.starts_with("fc") || ip.starts_with("fd") || ip.starts_with("fe80");
    }

    int oct1 = 0, oct2 = 0, oct3 = 0, oct4 = 0;
    if (sscanf_s(ip.c_str(), "%d.%d.%d.%d", &oct1, &oct2, &oct3, &oct4) == 4) {
        // 10.0.0.0/8
        if (oct1 == 10) return true;
        // 172.16.0.0/12
        if (oct1 == 172 && oct2 >= 16 && oct2 <= 31) return true;
        // 192.168.0.0/16
        if (oct1 == 192 && oct2 == 168) return true;
        // 169.254.0.0/16 (Link-local)
        if (oct1 == 169 && oct2 == 254) return true;
    }
    return false;
}

std::optional<ParsedIceCandidate> IceCandidatePrioritizer::parseSdpCandidate(const std::string& candidateLine) noexcept {
    std::string line = candidateLine;
    if (line.starts_with("candidate:")) {
        line = line.substr(10);
    } else if (line.starts_with("a=candidate:")) {
        line = line.substr(12);
    }

    std::istringstream iss(line);
    std::string foundation, compStr, transport, prioStr, ip, portStr, typKw, typStr;

    if (!(iss >> foundation >> compStr >> transport >> prioStr >> ip >> portStr >> typKw >> typStr)) {
        return std::nullopt;
    }

    ParsedIceCandidate c{};
    c.rawCandidate = candidateLine;
    c.foundation = foundation;
    try {
        c.component = static_cast<uint32_t>(std::stoul(compStr));
        c.priority = static_cast<uint32_t>(std::stoul(prioStr));
        c.port = static_cast<uint16_t>(std::stoul(portStr));
    } catch (...) {
        return std::nullopt;
    }

    std::string transLower = transport;
    std::transform(transLower.begin(), transLower.end(), transLower.begin(), [](unsigned char ch){ return static_cast<char>(::tolower(ch)); });
    c.transport = transLower;
    c.ip = ip;
    c.family = determineAddressFamily(ip);
    c.isLoopback = isLoopbackAddress(ip);
    c.isCgnat = isCgnatAddress(ip);
    c.isPrivateLan = isPrivateAddress(ip);

    if (typStr == "host") {
        c.type = CandidateType::Host;
    } else if (typStr == "srflx") {
        c.type = CandidateType::ServerReflexive;
    } else if (typStr == "prflx") {
        c.type = CandidateType::PeerReflexive;
    } else if (typStr == "relay") {
        c.type = CandidateType::Relayed;
    } else {
        c.type = CandidateType::Unknown;
    }

    std::string kw, val;
    while (iss >> kw >> val) {
        if (kw == "raddr") {
            c.relatedIp = val;
        } else if (kw == "rport") {
            try {
                c.relatedPort = static_cast<uint16_t>(std::stoul(val));
            } catch (...) {}
        }
    }

    return c;
}

const char* IceCandidatePrioritizer::candidateTypeToString(CandidateType type) noexcept {
    switch (type) {
        case CandidateType::Host:            return "host";
        case CandidateType::ServerReflexive: return "srflx";
        case CandidateType::PeerReflexive:   return "prflx";
        case CandidateType::Relayed:         return "relay";
        default:                             return "unknown";
    }
}

const char* IceCandidatePrioritizer::natTypeToString(NatType type) noexcept {
    switch (type) {
        case NatType::OpenInternetDirect:    return "Open Internet (Direct)";
        case NatType::FullCone:              return "Full Cone NAT";
        case NatType::AddressRestrictedCone: return "Address-Restricted Cone NAT";
        case NatType::PortRestrictedCone:    return "Port-Restricted Cone NAT";
        case NatType::Symmetric:             return "Symmetric NAT (Requires Relay)";
        case NatType::CarrierGradeNat:       return "Carrier-Grade NAT (CGNAT)";
        default:                             return "Unknown NAT";
    }
}

const char* IceCandidatePrioritizer::failureReasonToString(ConnectionFailureReason reason) noexcept {
    switch (reason) {
        case ConnectionFailureReason::None:                    return "None";
        case ConnectionFailureReason::SignalingUnreachable:    return "Signaling Server Unreachable";
        case ConnectionFailureReason::SignalingTimeout:        return "Signaling Negotiation Timeout";
        case ConnectionFailureReason::IceGatheringFailed:      return "ICE Candidate Gathering Failed";
        case ConnectionFailureReason::IceConnectivityTimeout:  return "ICE Connectivity Checks Timed Out";
        case ConnectionFailureReason::SymmetricNatMismatch:    return "Symmetric NAT Mismatch (Direct Connection Impossible without TURN)";
        case ConnectionFailureReason::FirewallBlocked:          return "Local or Gateway UDP Firewall Blocked";
        case ConnectionFailureReason::NetworkChanged:          return "Network Adapter Link Changed";
        case ConnectionFailureReason::PeerDisconnected:        return "Remote Peer Disconnected";
        default:                                               return "Unknown Failure";
    }
}

NatType IceCandidatePrioritizer::classifyNatBehavior(const std::vector<ParsedIceCandidate>& localCandidates) noexcept {
    if (localCandidates.empty()) return NatType::Unknown;

    bool hasHost = false;
    bool hasPrivateHost = false;
    bool hasSrflx = false;
    bool hasCgnatSrflx = false;
    uint16_t hostPort = 0;
    uint16_t srflxPort = 0;

    for (const auto& c : localCandidates) {
        if (c.type == CandidateType::Host) {
            hasHost = true;
            if (c.isPrivateLan) hasPrivateHost = true;
            hostPort = c.port;
        } else if (c.type == CandidateType::ServerReflexive) {
            hasSrflx = true;
            srflxPort = c.port;
            if (c.isCgnat) hasCgnatSrflx = true;
        }
    }

    if (hasCgnatSrflx) {
        return NatType::CarrierGradeNat;
    }

    if (hasHost && !hasPrivateHost && !hasSrflx) {
        return NatType::OpenInternetDirect;
    }

    if (hasSrflx) {
        if (hostPort != 0 && srflxPort != 0 && hostPort == srflxPort) {
            return NatType::FullCone;
        }
        return NatType::PortRestrictedCone;
    }

    return hasPrivateHost ? NatType::AddressRestrictedCone : NatType::Unknown;
}

ConnectionFailureReason IceCandidatePrioritizer::classifyFailure(
    bool signalingConnected,
    bool candidatesGathered,
    bool iceConnected,
    bool isSymmetricBothSides
) noexcept {
    if (!signalingConnected) {
        return ConnectionFailureReason::SignalingUnreachable;
    }
    if (!candidatesGathered) {
        return ConnectionFailureReason::IceGatheringFailed;
    }
    if (isSymmetricBothSides && !iceConnected) {
        return ConnectionFailureReason::SymmetricNatMismatch;
    }
    if (!iceConnected) {
        return ConnectionFailureReason::IceConnectivityTimeout;
    }
    return ConnectionFailureReason::None;
}

// -----------------------------------------------------------------------------
// PortMappingManager implementation
// -----------------------------------------------------------------------------

common::Result<PortMappingLease> PortMappingManager::requestPortMapping(
    uint16_t internalPort,
    uint16_t requestedExternalPort,
    const std::string& protocol,
    uint32_t leaseSeconds,
    const std::string& description
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if mapping already active
    for (const auto& lease : activeLeases_) {
        if (lease.externalPort == requestedExternalPort && lease.protocol == protocol && lease.isActive) {
            return lease;
        }
    }

    // Create session lease record with temporary TTL
    PortMappingLease lease{};
    lease.internalPort = internalPort;
    lease.externalPort = (requestedExternalPort > 0) ? requestedExternalPort : internalPort;
    lease.protocol = protocol;
    lease.leaseDurationSeconds = leaseSeconds;
    lease.leaseExpiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(leaseSeconds);
    lease.description = description;
    lease.isActive = true;

    activeLeases_.push_back(lease);
    MIST_LOG_INFO(common::SubsystemId::Transport,
                  "Port mapping leased: " + protocol + " internal " + std::to_string(internalPort) +
                  " -> external " + std::to_string(lease.externalPort) + " (TTL: " + std::to_string(leaseSeconds) + "s)");

    return lease;
}

common::Result<void> PortMappingManager::releasePortMapping(uint16_t externalPort, const std::string& protocol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove_if(activeLeases_.begin(), activeLeases_.end(), [&](const PortMappingLease& l) {
        return l.externalPort == externalPort && l.protocol == protocol;
    });

    if (it != activeLeases_.end()) {
        activeLeases_.erase(it, activeLeases_.end());
        MIST_LOG_INFO(common::SubsystemId::Transport, "Port mapping released: external " + std::to_string(externalPort) + " (" + protocol + ")");
        return common::Result<void>::success();
    }

    return common::Error(common::SubsystemId::Transport, common::ErrorCode::NotFound, "No active port mapping found for specified port");
}

void PortMappingManager::releaseAllMappings() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeLeases_.empty()) {
        MIST_LOG_INFO(common::SubsystemId::Transport, "Releasing all " + std::to_string(activeLeases_.size()) + " active port mappings on shutdown.");
        activeLeases_.clear();
    }
}

std::vector<PortMappingLease> PortMappingManager::getActiveLeases() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeLeases_;
}

bool PortMappingManager::hasActiveLease(uint16_t externalPort) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& l : activeLeases_) {
        if (l.externalPort == externalPort && l.isActive) return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// IceRestartController implementation
// -----------------------------------------------------------------------------

IceRestartController::IceRestartController(uint32_t maxRestarts) noexcept
    : maxRestarts_(maxRestarts) {}

bool IceRestartController::canAttemptRestart() const noexcept {
    if (restartCount_ >= maxRestarts_) return false;
    auto now = std::chrono::steady_clock::now();
    if (restartCount_ > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRestartTime_).count();
        if (elapsed < 1000) { // Enforce minimum 1000ms backoff between ICE restarts
            return false;
        }
    }
    return true;
}

void IceRestartController::recordRestartAttempt() noexcept {
    restartCount_++;
    iceGeneration_++;
    lastRestartTime_ = std::chrono::steady_clock::now();
    MIST_LOG_INFO(common::SubsystemId::Transport,
                  "ICE restart recorded (attempt " + std::to_string(restartCount_) +
                  "/" + std::to_string(maxRestarts_) + ", generation " + std::to_string(iceGeneration_) + ")");
}

void IceRestartController::reset() noexcept {
    restartCount_ = 0;
    iceGeneration_ = 0;
}

} // namespace mist::transport
