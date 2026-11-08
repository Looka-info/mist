# Mist — Milestone 7 Implementation Plan
## Production Field Validation, Network Reliability & Release Candidate Qualification

**Document Status:** Approved Architecture Plan  
**Target Version:** Mist v0.2.0-rc1  
**Hardware Baseline:** AMD Ryzen 5 5600, NVIDIA GeForce GTX 1660 SUPER, 16 GB RAM, Windows 11 Pro x64  
**Date:** November 2026  

---

## 1. Executive Summary & Audit Findings

Milestone 7 transitions Mist from feature construction to production hardening, metric reconciliation, realistic network validation, and release candidate qualification. An evidence-based audit of Milestone 0–6 code and benchmark telemetry revealed three critical accounting and behavior findings that must be corrected:

### 1.1 Finding A: Audio Packet Cadence Deficit
- **Observation:** In the Milestone 6 benchmark (10.01 seconds), host audio packets sent totaled 727 packets (~72.6 pps) rather than the nominal 1,000 packets expected from 10 ms Opus framing (100 pps).
- **Root Cause Audit:** In `engine/audio/src/WasapiAudioCapture.cpp`, `readPacket(timeoutMs = 20)` calls `WaitForSingleObject(hCaptureEvent_, 10)`. When no external application is rendering sound to the Windows default endpoint, loopback capture relies on timeout fallback. Because the timeout was set to 20 ms and each timeout iteration yielded exactly one 10 ms frame (480 samples), the effective generation cadence was choked to ~50–75 pps.
- **Resolution:** Refactor `WasapiAudioCapture` silence handling to accurately track monotonic time and generate silent Opus frames at strict 10 ms intervals (100 pps) during idle system periods.

### 1.2 Finding B: Video Received vs. Decoded Discrepancy
- **Observation:** Host encoded 601 frames, client received 511 frames, and client decoded 451 frames (a 60-frame discrepancy between received and decoded).
- **Root Cause Audit:**
  1. *Signaling / ICE Race:* Host `CaptureEncodePipeline` started generating frames immediately upon `connect()`. During the ~1.5 s ICE candidate gathering and DTLS handshake, ~90 frames were transmitted before the peer connection data path was fully established.
  2. *Initial Reference Frame Dependency:* The first frames received by the client lacked an IDR keyframe, causing Media Foundation MFT to buffer without decoding until the first RTCP PLI keyframe arrived.
  3. *Single-Frame Drain Defect:* In `ClientApplication.cpp`, `rtcTransport.setFrameReceivedCallback` called `decoder.getNextFrame()` exactly once per incoming access unit. When the hardware MFT decoder produced more than one frame from a packet submission, subsequent frames accumulated in `decodedQueue_` (max 2 frames) and were silently dropped by queue overflow protection.
  4. *Missing EOF Drain:* When the benchmark duration elapsed, the client loop broke immediately without flushing or draining the decoder transform, stranding in-flight frames.
- **Resolution:** In `ClientApplication.cpp`, implement a complete drain loop (`while (auto frameRes = decoder.getNextFrame())`) on each packet submission and execute a final `flushAndDrain()` upon session shutdown. Introduce explicit counter metrics for submitted frames, decoded frames, and queue drops.

### 1.3 Finding C: Configured vs. Measured Bitrate Discrepancy
- **Observation:** Balanced profile configured 8,000 kbps, but measured video bitrate was ~1,862 kbps.
- **Root Cause Audit:** `SyntheticFrameGenerator` cleared the Direct3D 11 render target with a uniform solid color (`ClearRenderTargetView`). In H.264 video compression, a solid flat color field has near-zero spatial entropy. Under NVENC low-latency CBR/VBR without filler data padding, the quantizer reaches minimum QP (highest quality) with only ~1.8 Mbps of bitstream data.
- **Resolution:** Enhance `SyntheticFrameGenerator` to generate a high-entropy, realistic animated scene with dynamic geometric shapes, moving high-contrast textures, text overlays, and procedural motion vectors to saturate the target bitrate.

---

## 2. Workstream Breakdown

```
┌────────────────────────────────────────────────────────────────────────┐
│                      MILESTONE 7 WORKSTREAMS                           │
├────────────────────┬────────────────────┬──────────────────────────────┤
│ 1. Telemetry &     │ 2. QoS & Bandwidth │ 3. NAT Traversal &           │
│    Accounting      │    Adaptation      │    TURN Verification         │
│ - Unambiguous      │ - Realistic test   │ - Real TURN relay            │
│   counter models   │   patterns         │   credentials                │
│ - Fixed audio loop │ - Controlled loss  │ - Candidate pair             │
│   cadence (100pps) │   & RTT emulation  │   classification             │
│ - Video drain loop │ - Convergence      │ - Direct LAN / WAN /         │
│   & queue drops    │   metrics logging  │   Relayed TURN breakdown     │
├────────────────────┼────────────────────┼──────────────────────────────┤
│ 4. Long-Duration   │ 5. Security &      │ 6. Release Candidate         │
│    Stability       │    Pairing Audit   │    Qualification             │
│ - 10-min benchmark │ - Host-enforced    │ - Version v0.2.0-rc1         │
│ - 30-min run       │   lockout boundary │ - Clean installer testing    │
│ - 60-min run /     │ - Constant-time    │ - Production Authenticode    │
│   blocker status   │   pairing check    │   signing architecture       │
│ - Bounded VRAM/RAM │ - DPAPI / secure   │ - Unified field-test         │
│   leak checks      │   store audit      │   reporting schema           │
└────────────────────┴────────────────────┴──────────────────────────────┘
```

### Workstream 1: Metric Reconciliation & Accounting Disambiguation
- Define unified, unambiguous telemetry counters:
  - **VIDEO:** `encodedFrames`, `rtpPacketsSent`, `rtpBytesSent`, `rtpPacketsReceived`, `completeAccessUnitsReceived`, `framesSubmittedToDecoder`, `framesDecoded`, `framesPresented`, `framesQueueDiscarded`, `framesLost`, `decoderErrors`.
  - **AUDIO:** `pcmBlocksCaptured`, `opusPacketsGenerated`, `rtpPacketsSent`, `rtpPacketsReceived`, `opusPacketsDecoded`, `pcmFramesRendered`, `plcEvents`, `fecRecoveries`, `playbackUnderruns`.
  - **NETWORK:** `bytesSent`, `bytesReceived`, `packetLossFraction`, `rttMs`, `jitterMs`, `bandwidthEstimateKbps`.
- Update `WasapiAudioCapture.cpp` to maintain a monotonic clock for silence pacing at exactly 10 ms (100 pps).
- Update `ClientApplication.cpp` to drain all frames from `decoder.getNextFrame()` and execute an end-of-session flush.
- Update `SyntheticFrameGenerator` with animated high-frequency patterns.

### Workstream 2: Adaptive Bitrate & Network Impairment Verification
- Build automated controlled impairment tests:
  - High-bandwidth unconstrained link (25 Mbps).
  - Constrained links (10 Mbps, 5 Mbps, 2 Mbps).
  - Packet loss injection (2% light loss, 6% congestion loss, 15% critical loss).
  - RTT jitter injection.
- Validate the complete AIMD cycle:
  `Stable` $\rightarrow$ `Loss Detected` $\rightarrow$ `Congested (20% Cut)` $\rightarrow$ `Critical Congestion (50% Cut)` $\rightarrow$ `Cooldown` $\rightarrow$ `Additive Recovery`.
- Measure and record convergence time, oscillation frequency, and NVENC in-flight reconfiguration success.

### Workstream 3: Real TURN Relay Traversal
- Support external TURN relay configuration via CLI and configuration file (`--turn-server`, `--turn-username`, `--turn-password`).
- Test candidate pair selection: verify `rtc::PeerConnection::getSelectedCandidatePair()` reports `RelayedTurn`.
- Validate bi-directional media (1080p60 video, 48kHz audio, low-latency input) routed over the TURN relay.
- Report Direct LAN, Direct WAN, and Relayed TURN benchmarks distinctly.

### Workstream 4: Stability & Long-Duration Streaming
- Execute sustained streaming sessions:
  - 10-minute full tri-stream benchmark (video + audio + input).
  - 30-minute endurance session.
  - 60-minute stability run (measuring memory, GPU VRAM, CPU utilization, thread handles, audio underruns).
- Log periodic snapshots every 30 seconds to monitor memory leak linearity.

### Workstream 5: Pairing & Security Audit
- Verify rate limiting (5 failed attempts $\rightarrow$ 300-second lockout) at the authoritative host engine boundary.
- Confirm constant-time string comparisons (`constantTimeEquals`) protect against timing attacks.
- Verify granular capability enforcement: paired devices have input permissions disabled by default.
- Test pairing abuse edge cases: expired codes, reused codes, concurrent requests, malformed payloads.

### Workstream 6: Release Candidate Packaging & Windows Qualification
- Bump version to `0.2.0-rc1`.
- Update packaging script (`scripts/build-installer.ps1`) to produce `dist/mist-v0.2.0-rc1-windows-x64.zip`.
- Test installer and uninstaller lifecycle on a clean target path without administrator elevation.
- Document production Authenticode code-signing procedure vs development certificate transparently.
- Generate unified `artifacts/field-test-report.json`.

---

## 3. Implementation Order

1. **Telemetry & Audio/Video Fixes:**
   - Enhance `SyntheticFrameGenerator` with complex animated graphics.
   - Refactor `WasapiAudioCapture` silence pacing.
   - Refactor `ClientApplication` video decoder draining loop and end-of-stream flush.
2. **Telemetry Counters & Accounting:**
   - Update `HostApplication` and `ClientApplication` report structures with unambiguous counter names.
3. **QoS Adaptation & Network Impairment Tests:**
   - Implement simulated impairment test in unit test suite (`TestCongestionControl.cpp`).
   - Validate live NVENC reconfiguration across 2M, 5M, 8M, 15M targets.
4. **TURN Relay Infrastructure & Testing:**
   - Add CLI options for TURN credentials to host, client, and desktop apps.
   - Validate candidate pair classification.
5. **Stability & Long-Duration Benchmark Execution:**
   - Run 10-minute, 30-minute, and 60-minute sessions; collect performance metrics.
6. **Security & Pairing Audit:**
   - Verify host-side rate limiter and add pairing abuse unit tests.
7. **Release Candidate Packaging & Documentation:**
   - Bump version to `0.2.0-rc1`.
   - Update installer scripts, release manifest, and all required documentation files.
