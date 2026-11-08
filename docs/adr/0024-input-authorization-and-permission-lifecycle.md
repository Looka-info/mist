# ADR 0024: Input Authorization and Permission Lifecycle

## Status
Accepted

## Context
Exposing low-level keyboard, mouse, and game controller injection on a host machine creates critical security, safety, and operational risks. Unauthorized or unmonitored remote input can allow attackers to hijack host sessions, manipulate applications, or cause disruption. The host operator must always retain ultimate sovereignty over their physical machine, and permissions must be strictly scoped, validated, rate-limited, and revocable at any time.

## Decision
1. **Explicit Multi-State Permission Lifecycle:**
   - Input injection is disabled by default (`InputPermissionState::Disabled`).
   - Remote input requires affirmative authorization via a four-state machine:
     - `Disabled`: Default state. All remote input packets are discarded immediately.
     - `Requested`: Remote client requests input capabilities via `PermissionMessage`.
     - `Granted`: Host administrator or local user approves input control. Feature mask determines allowed device classes (Keyboard, Mouse, Gamepad).
     - `Revoked`: Input control terminated. Triggers immediate emergency input release.
2. **Local Host Override (Host Sovereignty):**
   - The physical host operator always maintains absolute precedence.
   - If physical host keyboard or mouse activity is detected, remote input injection is temporarily suspended or completely revocable by the host operator.
   - Local CLI/GUI controls allow immediate revocation (`revokePermissions()`) at any time.
3. **Session Binding & Anti-Spoofing:**
   - Every input packet must carry a valid session identifier matching the active WebRTC peer.
   - Cross-session injection attempts are immediately rejected and logged as security anomalies.
4. **Sliding-Window Rate Limiting:**
   - To guard against denial-of-service or packet flooding from compromised clients, input packets are subject to a sliding-window rate limiter (enforcing a maximum of 2,000 events/second per session).
   - Excess packets are dropped and recorded in telemetry metrics (`remote_input_dropped_events`).
5. **Emergency State Release on Interruption:**
   - If the network connection drops, the session terminates, or permissions are revoked, `releaseAllKeys()` and `releaseAllButtons()` are invoked unconditionally, clearing all held synthetic keys and buttons.

## Consequences
- Remote input cannot be injected without explicit host consent.
- Host operator retains ultimate physical control over the machine.
- Flooding or malformed packet attacks are neutralized at the dispatcher boundary.
- Zero risk of persistent stuck keys following disconnects or revocation.
