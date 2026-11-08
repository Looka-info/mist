# ADR 0030: STUN/TURN Relay Selection and Candidate Pair Reporting

## Status
Accepted

## Context
Deploying WebRTC streaming across diverse network topologies (LANs, symmetric NATs, mobile networks, university/corporate firewalls) requires both direct peer-to-peer traversal via STUN and guaranteed fallback relay via authenticated TURN servers. The system must also truthfully report whether a live connection is direct or relayed.

## Decision
1. **Authenticated TURN Relay Configuration:**
   - Support `turn:` and `turns:` URI schemes with username and password authentication in `WebRtcConfig`.
   - Support UDP, TCP, and TLS relay options (`TurnUdp`, `TurnTcp`, `TurnTls`) mapped into `rtc::IceServer`.
   - Separate STUN discovery from media relay: signaling servers are strictly session coordinators, never media relays.
2. **Active Candidate Pair Query & Classification:**
   - Query `pc_->getSelectedCandidatePair(&localCand, &remoteCand)` on the active `rtc::PeerConnection`.
   - Classify connection type:
     - `DirectLan`: Local and remote candidate types are both `Host`.
     - `DirectWan`: Local or remote candidate types are `ServerReflexive` (`srflx`) or `PeerReflexive` (`prflx`).
     - `RelayedTurn`: Local or remote candidate type is `Relayed` (`relay`).
3. **Truthful Telemetry Reporting:**
   - Expose connection type in `getStatsSnapshot()`, CLI diagnostics (`--network-info`), and desktop UI.

## Consequences
- Guaranteed streaming connectivity even behind restrictive symmetric NATs.
- Instant user visibility into direct vs relayed path status.
- Zero ambiguity between public relay transport and direct P2P connections.
