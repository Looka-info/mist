# Mist — Milestone 3 Implementation Plan
## Native WebRTC Transport & Session Signaling

**Author:** Principal C++ Networking & Real-Time Media Engineer  
**Date:** November 8, 2026  
**Status:** In Progress  

---

## 1. Objectives & Scope

The goal of Milestone 3 is to connect Mist's existing host capture/encoding pipeline (Milestone 1) and client hardware decoding/presentation pipeline (Milestone 2) into a unified, ultra-low-latency real-time network streaming system using native WebRTC and a dedicated signaling service.

### Strict Scope Boundaries
- **In Scope:**
  - Critical preliminary NVENC SDK vs. runtime driver compatibility audit.
  - Frame throughput timing analysis and high-precision animated GPU test pattern.
  - Native integration of `libdatachannel` for WebRTC peer connections, H.264 video tracks, and DataChannels.
  - RFC 6184 H.264 RTP packetization and depacketization with 90kHz video clock.
  - End-to-end asynchronous bounded media pipelines for host and client.
  - WebRTC DataChannels for `session-control` and `diagnostics`.
  - RTCP feedback (PLI keyframe requests, NACK, receiver reports).
  - Standalone Rust signaling service (`services/signaling`) and C++ `ISignalingClient` integration.
  - ICE, STUN, and TURN relay support with local loopback and LAN testing.
  - Comprehensive network telemetry and latency instrumentation.
  - Diagnostic CLI commands for host and client.
  - Complete unit, integration, and stability test suites.
- **Out of Scope (Deferred to Future Milestones):**
  - Audio streaming (WASAPI loopback capture & playback — Milestone 4).
  - Remote keyboard, mouse, and gamepad input injection (Milestone 5).
  - Account management, cloud dashboards, payment processing.

---

## 2. Technical Decisions & Subsystem Deliverables

### 2.1 WebRTC Transport Library Selection (ADR 0010)
- **Selected Library:** `libdatachannel` (pinned stable release `v0.22.4`).
- **Rationale:** Native C++17/C++20, clean CMake integration, zero external heavyweight runtime dependencies, built-in MbedTLS cryptographic provider, native support for `rtc::Track` H.264 packetization/depacketization, RTCP SR/RR, and asynchronous WebSocket client.
- **Abstraction:** Encapsulated behind `mist::transport::ITransport` and `mist::transport::webrtc` so no `libdatachannel` types leak into existing capture/encode/decode/render headers.

### 2.2 H.264 RTP Packetization & Media Pipeline (ADR 0011)
- **RFC 6184 Compliance:** Non-interleaved packetization mode with 90kHz RTP timestamp clock.
- **NAL Unit Handling:** Long start code sequence (`00 00 00 01`) handling, single-NAL packaging for small slices (AUD, SPS, PPS), and FU-A fragmentation for macroblock slice data exceeding network MTU (1200 bytes).
- **Source Timestamping:** RTP timestamps mapped directly from host QPC acquisition timestamps, preserving frame pacing without artificial jitter.
- **Keyframe Recovery:** Host listens for RTCP Picture Loss Indication (PLI) packets and immediately requests an IDR keyframe from `NvencVideoEncoder`.

### 2.3 Signaling Architecture & Protocol (ADR 0012)
- **Service:** Standalone asynchronous Rust microservice under `services/signaling` using `tokio`, `tokio-tungstenite`, and `serde`.
- **Protocol:** Versioned JSON WebSocket messages exchanging session registration, SDP offers, SDP answers, and trickle ICE candidates.
- **Authentication:** Short-lived `AuthToken` verification against high-entropy session credentials before peer pairing.

### 2.4 ICE & Relay Strategy (ADR 0013)
- **Gathering:** Host and client configure STUN (Google/Cloudflare default test servers) and configurable TURN relays.
- **Direct First:** Prioritize host/client direct host and server-reflexive candidates for minimal latency on LAN.
- **Relay Fallback:** Seamless fallback to TURN relay when NAT traversal fails.

### 2.5 Low-Latency Bounded Buffering (ADR 0014)
- **Host Transport Queue:** Maximum 2 pending encoded packets. Drops oldest packet if network backpressure occurs.
- **Client Playout Queue:** Maximum 2 pending reassembled access units. Drops oldest frame if hardware decoder falls behind.
- **Zero Disk I/O:** Depacketized access units feed directly into memory buffers for `D3D11VideoDecoder`.

---

## 3. Implementation Phases & Sequence

1. **Phase 1: Architecture & Decision Records**
   - Author ADRs 0010, 0011, 0012, 0013, 0014.
2. **Phase 2: Preliminary Compatibility & Frame Throughput Audit**
   - Implement `NvencVideoEncoder::queryRuntimeAudit()` extracting loaded DLL path, DLL version, runtime max API version vs. header version.
   - Investigate M1 frame count; implement high-precision QPC-paced animated GPU test pattern.
3. **Phase 3: WebRTC Dependency Integration**
   - Configure `libdatachannel` via CMake `FetchContent` with `USE_MBEDTLS=ON` and `NO_MEDIA=OFF`.
4. **Phase 4: WebRTC Transport Implementation (`engine/transport/webrtc`)**
   - Implement `WebRtcTransport`, `PeerConnectionManager`, `VideoTrackSender`, `VideoTrackReceiver`, `DataChannelManager`, `IceConnectionManager`, `TransportStatistics`.
5. **Phase 5: Host & Client Media Pipeline Connection**
   - Wire `CaptureEncodePipeline` to `VideoTrackSender`.
   - Wire `VideoTrackReceiver` to `D3D11VideoDecoder` and `D3D11Renderer`.
6. **Phase 6: Rust Signaling Service (`services/signaling`)**
   - Implement WebSocket server, room pairing, message validation, token verification.
   - Wire C++ `WebSocketSignalingClient` using `rtc::WebSocket`.
7. **Phase 7: CLI Diagnostics**
   - Add `--transport-info`, `--webrtc-test`, `--stream-test`, `--stream-live`, `--receive-test`, `--stream-receive`.
8. **Phase 8: Tests & Hardware Verification**
   - Author unit, integration, and stability test suites.
   - Run sustained 1080p60 10-minute test and emit `artifacts/milestone-3.json`.
   - Create `scripts/verify-milestone-3.ps1`.
9. **Phase 9: Documentation & Completion Report**
   - Update architecture, networking, protocol, security, and testing docs.
   - Author `docs/milestone-3-report.md`.
