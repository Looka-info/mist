# ADR 0003: Subsystem Modular Boundaries

## Status
Accepted

## Context
High-performance streaming applications typically suffer from tight coupling between capture, encoding, transport, and presentation layers. Unchecked coupling prevents mocking during unit testing and complicates hardware backend changes.

## Decision
1. **Engine Separation:** Separate the engine into single-purpose modular libraries:
   - `engine/common`: Error, Result, SubsystemId, Logger, Config.
   - `engine/session`: State machine and session lifecycle.
   - `engine/capture`: Capture contracts (`ICaptureEngine`).
   - `engine/encode`: Video encoding contracts (`IVideoEncoder`).
   - `engine/decode`: Video decoding contracts (`IVideoDecoder`).
   - `engine/render`: Frame presentation contracts (`IRenderer`).
   - `engine/transport`: Network communication contracts (`ITransport`).
   - `engine/audio`: Audio capture and playback contracts (`IAudioCapture`, `IAudioPlayback`).
   - `engine/input`: Input injection contracts (`IInputManager`).
   - `engine/telemetry`: Metrics and diagnostics (`MetricRegistry`, `Counter`, `Gauge`, `LatencyHistogram`).
   - `engine/security`: Authentication and constant-time cryptography primitives (`ISecurityProvider`).
   - `services`: Signaling contracts (`ISignalingClient`).
2. **Interface Isolation for Future Milestones:** For Milestone 0, capture, encode, decode, render, transport, audio, and input are strictly interfaces. No premature implementation code is allowed ahead of its designated milestone.
3. **Application Separation:** Separate executables for `apps/host` and `apps/client`, with a shared `apps/desktop` integration boundary.

## Consequences
- Every module has a single clear purpose.
- Hardware implementations (NVENC, DXGI, WASAPI) can be slotted in during future milestones behind verified interface boundaries.
- Unit testing can mock any hardware or network subsystem without real physical devices.
