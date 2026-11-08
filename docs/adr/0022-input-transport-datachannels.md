# ADR 0022: Dual WebRTC DataChannels for Low-Latency Input Transport

## Status
Accepted

## Context
Interactive remote gaming demands two distinct transport characteristics for user input:
1. **Lossless, strictly ordered delivery** for state transitions (e.g. key down, key up, mouse click, permission messages). Dropping a key-release packet results in a catastrophically stuck key on the host.
2. **Minimum-latency, loss-tolerant delivery** for high-frequency continuous motion (e.g. 500–1000 Hz mouse movement, gamepad analog sticks). Retransmitting stale mouse coordinates causes unplayable cursor lag and bufferbloat.

## Decision
1. **Dual SCTP DataChannel Architecture:**
   - Reuse the existing `libdatachannel` WebRTC PeerConnection to establish two dedicated SCTP channels:
     - `input-events`: Configured as **Reliable, Ordered**. Carries discrete keyboard transitions (`KeyEvent`), mouse button transitions (`MouseButtonEvent`), permission messages (`PermissionMessage`), and emergency resets (`ResetState`).
     - `input-state`: Configured as **Unordered, Time-Bounded** (`maxPacketLifeTime = 50ms`). Carries continuous mouse motion (`MouseMoveEvent`) and analog controller updates (`GamepadState`).
2. **Channel Separation from Authorization:**
   - Establishing a WebRTC DataChannel connection does not automatically grant input control. The host input dispatcher rejects all incoming packets until explicit session authorization and host permission have been verified.
3. **Network Thread Decoupling:**
   - WebRTC DataChannel reception callbacks never invoke Windows input APIs directly. Received packets are validated and enqueued into a thread-safe bounded dispatch queue.

## Consequences
- Guaranteed prevention of stuck keys or mouse buttons under packet loss.
- Zero latency accumulation or bufferbloat during rapid mouse sweeps.
- Clean separation between transport-layer availability and application-layer control authorization.
