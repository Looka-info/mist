# ADR 0013: ICE Candidate Gathering and Relay Strategy

## Status
Accepted

## Context
Interactive remote gaming demands the shortest possible network path between host and client. In LAN environments, direct host-to-host UDP connections must be used to achieve sub-millisecond network transit times. Across WAN and firewalled networks, NAT traversal via STUN and fallback to TURN relays are required.

## Decision
1. **Interactive Connectivity Establishment (ICE):**
   - Use standard ICE candidate gathering (host, server-reflexive, and relay).
   - Support trickle ICE: send candidates over the signaling channel as soon as they are gathered rather than waiting for complete gathering to finish.
2. **Prioritization Policy:**
   - Prefer direct host IP candidates on the local subnet (wired LAN / WiFi 6) to achieve the absolute lowest latency.
   - Use STUN (`stun:stun.l.google.com:19302`, `stun:stun.cloudflare.com:3478`) for NAT endpoint discovery in WAN settings.
   - Configure optional authenticated TURN servers (`turn:relay.example.com:3478`) strictly as a fallback when symmetric NAT or enterprise firewalls block direct UDP.
3. **Diagnostics & Telemetry:**
   - Track and expose the selected candidate pair type (`direct_lan`, `srflx_wan`, `relay_turn`).
   - Monitor ICE connection state transitions (`New`, `Checking`, `Connected`, `Completed`, `Failed`, `Disconnected`).

## Consequences
- Zero-latency penalty on LAN: packets flow directly point-to-point without hairpinning through a gateway or relay.
- Reliable fallback across restrictive routers and double-NAT configurations.
