# ADR 0032: Native Desktop Management UI Architecture

## Status
Accepted

## Context
Mist requires a clean, native Windows desktop application for managing hosts, pairing client devices, configuring streaming profiles, and monitoring live telemetry. The UI must be lightweight, fast, and avoid bloated web-based desktop wrappers (e.g. Electron) that consume excessive RAM and VRAM needed by games.

## Decision
1. **Native C++20 Desktop Shell (`apps/desktop`):**
   - Implemented as a lightweight native Windows C++ application linking directly to `mist_security`, `mist_transport`, and `mist_common`.
   - Adheres to Per-Monitor V2 DPI awareness (`initializeProcessDpiAwareness`).
2. **Six Core Operational Screens:**
   - **Screen A (HOME):** Discovered hosts, local host state, quick connection actions.
   - **Screen B (PAIRING):** Active 6-character code, expiration timer, host approval, fingerprint display.
   - **Screen C (HOST):** Hosting status, active viewer, permission policies, physical override status.
   - **Screen D (STREAM SETTINGS):** Resolution, target FPS, bitrate ranges, quality profile selection, VSync toggle.
   - **Screen E (LIVE SESSION):** Real-time tri-stream diagnostics (video FPS/bitrate/latency, network RTT/jitter/loss, audio underruns/skew, input rate).
   - **Screen F (DEVICE MANAGEMENT):** Trusted devices table, permission flags, device revocation.
3. **Dual Execution Mode:**
   - Supports non-interactive CLI diagnostic automation (`--self-test`, `--home`, `--pairing`, `--settings`, `--live-session`, `--devices`) for CI and scripted testing.
   - Supports interactive execution (`--ui` or default invocation).

## Consequences
- Zero web runtime or multi-hundred-megabyte framework dependencies.
- Sub-50ms application startup and < 20 MB memory footprint.
- Directly binds to real engine telemetry without inter-process IPC overhead.
