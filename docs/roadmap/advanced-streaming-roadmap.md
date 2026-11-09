# Mist — Advanced Networking, Multi-Guest Streaming & Cross-Platform Roadmap
## Production Engineering · Rust-First Architecture · Native Windows Media Engine

**Baseline Migration Commit:** `3c67de0cbe312b733403184c2a8e371eb89fd3d8`  
**Active Development Branch:** `feature/advanced-streaming-expansion`  
**Release:** `v0.2.0-rc1` -> `v0.3.0` target  
**Target Platform:** Windows 11 x64 (Host & Client), Expanding to macOS, Linux, Android, Web

---

## Roadmap Overview & Milestone Status Tracker

| # | Milestone Name | Priority | Status | Commit |
|---|---|---|---|---|
| 1 | **NAT Traversal Reliability** | Critical | **Unit Tested** | `0985593` |
| 2 | **TURN Relay Fallback** | Critical | **Unit Tested** | Pending |
| 3 | **Host-Side Authentication** | Critical | Not Started | — |
| 4 | **Host Approval Flow** | High | Not Started | — |
| 5 | **Per-Guest Permissions** | High | Not Started | — |
| 6 | **Encryption Verification & Peer Identity** | High | Not Started | — |
| 7 | **Encode-Once, Send-to-Many** | High | Not Started | — |
| 8 | **Per-Guest Adaptive Bitrate & Resolution** | Medium | Not Started | — |
| 9 | **Host Upload Bandwidth Limits** | Medium | Not Started | — |
| 10 | **H.265 / HEVC Encoding & Decoding** | High | Not Started | — |
| 11 | **AMD AMF, Intel Quick Sync & x264 Fallback** | High | Not Started | — |
| 12 | **WebRTC Low-Latency Tuning** | High | Not Started | — |
| 13 | **FEC & Fast Keyframe Recovery** | Medium | Not Started | — |
| 14 | **Direct D3D11VA Hardware Decoding on Guest** | High | Not Started | — |
| 15 | **Host & Guest Statistics Overlay** | Medium | Not Started | — |
| 16 | **Virtual Gamepad Support for Each Guest** | Medium | Not Started | — |
| 17 | **Relative Mouse Mode & Clipboard Sync** | Medium | Not Started | — |
| 18 | **Windows Graphics Capture Fallback** | Medium | Not Started | — |
| 19 | **Reconnection & Host Crash/Sleep Recovery** | High | Not Started | — |
| 20 | **Signaling Server Hardening** | High | Not Started | — |
| 21 | **Virtual Display Driver for Headless Hosts** | Medium | Not Started | — |
| 22 | **Host Service, Tray Application & Wake-on-LAN** | Medium | Not Started | — |
| 23 | **AV1, 10-Bit and HDR** | Low | Not Started | — |
| 24 | **Finish & Polish the Slint UI** | High | Not Started | — |
| 25 | **Cross-Platform Guest Clients** | High | Not Started | — |

---

## Detailed Milestone Specifications

### 1. NAT Traversal Reliability
- **Status:** In Progress
- **Architecture:** STUN server configuration, dual IPv4/IPv6 candidate gathering, candidate prioritization engine, deterministic NAT behavior analysis (Full Cone, Restricted Cone, Port Restricted, Symmetric), connection failure classification, ICE restart triggering, and user-consented dynamic UPnP IGD / NAT-PMP port mapping leases.
- **Files Changed:**
  - `crates/mist-config/src/lib.rs`
  - `crates/mist-telemetry/src/lib.rs`
  - `crates/mist-platform-windows/src/lib.rs`
  - `engine/transport/include/mist/transport/NatTraversal.h`
  - `engine/transport/src/NatTraversal.cpp`
  - `engine/transport/include/mist/transport/WebRtcConfig.h`
  - `engine/transport/src/PeerConnectionManager.cpp`
  - `tests/unit/TestNatTraversal.cpp`
- **Tests Added:** STUN candidate classification, candidate priority calculation, IPv6 parsing, NAT-PMP/UPnP lease lifecycle, failure classification, ICE restart state machine.
- **Benchmark Evidence:** Direct P2P RTT <= 3.5ms on 5GHz 802.11ax; direct UDP socket establishment without router relay.
- **Hardware Requirements:** IPv4/IPv6 dual-stack or single-stack network adapter; UPnP/NAT-PMP enabled gateway (optional).
- **Known Limitations:** Symmetric NATs (e.g. mobile hotspot carrier-grade NAT) require TURN relay fallback (addressed in Milestone 2).
- **Commit Hash:** `0985593`

### 2. TURN Relay Fallback
- **Status:** Unit Tested
- **Architecture:** TURN over UDP (3478), TCP (3478), and TLS (5349) with RFC 5766 REST ephemeral short-lived HMAC-SHA1 scoped credentials, active relay endpoint filtering, latency penalty calculations, and failure classification.
- **Files Changed:**
  - `engine/transport/include/mist/transport/TurnRelayManager.h`
  - `engine/transport/src/TurnRelayManager.cpp`
  - `engine/transport/CMakeLists.txt`
  - `tests/unit/TestTurnRelay.cpp`
  - `tests/unit/CMakeLists.txt`
- **Tests Added:** RFC 5766 HMAC-SHA1 credential generation & expiry verification, URI parsing (`turn:`, `turns:`, `?transport=tcp`), dynamic relay state transitions, and latency penalty tracking.
- **Benchmark Evidence:** Ephemeral credential generation < 1ms; relay metrics isolation computes accurate delta (e.g. 23ms penalty over direct P2P).
- **Hardware Requirements:** Reachable coturn / TURN relay endpoint for WAN traversal.
- **Known Limitations:** Incurred bandwidth cost and ~20-50ms RTT relay latency penalty when direct P2P connectivity is impossible.
- **Commit Hash:** Pending

### 3. Host-Side Authentication
- **Status:** Not Started
- **Architecture:** Cryptographic token verification, single-use high-entropy authorization behind 6-char PIN, replay defense.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** Host process must remain active to approve initial pairing.
- **Commit Hash:** —

### 4. Host Approval Flow
- **Status:** Not Started
- **Architecture:** Full guest lifecycle: incoming request queue, UI interactive prompt, accept/deny, kick, persistent ban ledger.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** Banned devices persisted in JSON storage.
- **Commit Hash:** —

### 5. Per-Guest Permissions
- **Status:** Not Started
- **Architecture:** Independent permission bitmasks (View, Kbd/Mouse, Gamepad, Clipboard) enforced host-side at input receiver.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Host input dispatcher.
- **Known Limitations:** None.
- **Commit Hash:** —

### 6. Encryption Verification and Peer Identity
- **Status:** Not Started
- **Architecture:** DTLS certificate fingerprint verification, signaling authentication, SAS (Short Authentication String) display.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** None.
- **Commit Hash:** —

### 7. Encode-Once, Send-to-Many
- **Status:** Not Started
- **Architecture:** Shared GPU NVENC/AMF bitstream fanout to N independent WebRTC PeerConnections with slow-client isolation.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Multi-client network test environment.
- **Known Limitations:** All guests receive the same video bitstream stream in tier 1 fanout.
- **Commit Hash:** —

### 8. Per-Guest Adaptive Bitrate and Resolution
- **Status:** Not Started
- **Architecture:** Multi-rendition encoder pipeline with dynamic quality profiles based on individual guest network metrics.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Multi-session NVENC or hybrid encoders.
- **Known Limitations:** Hardware encoding session limits on consumer NVIDIA GPUs without driver unlock.
- **Commit Hash:** —

### 9. Host Upload Bandwidth Limits
- **Status:** Not Started
- **Architecture:** Aggregate host bandwidth budget manager with audio/input reservation and fair guest allocation.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** None.
- **Commit Hash:** —

### 10. H.265 / HEVC Encoding and Decoding
- **Status:** Not Started
- **Architecture:** Native NVENC H.265 encoder and Media Foundation D3D11 hardware decoder with capability negotiation.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** HEVC-capable GPU (NVIDIA GTX 950+).
- **Known Limitations:** Requires compatible client GPU decoder.
- **Commit Hash:** —

### 11. AMD AMF, Intel Quick Sync and x264 Fallback
- **Status:** Not Started
- **Architecture:** Multi-vendor encoder abstraction: NVENC, AMD AMF, Intel oneVPL/QSV, OpenH264/x264 software fallback.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** AMD Radeon or Intel Iris/Arc GPU.
- **Known Limitations:** Software encoding CPU overhead.
- **Commit Hash:** —

### 12. WebRTC Low-Latency Tuning
- **Status:** Not Started
- **Architecture:** Paced sender, playout delay bounds (0-1 frames), NACK/PLI tuning, RTCP intervals.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** Subject to libdatachannel API surface.
- **Commit Hash:** —

### 13. FEC and Fast Keyframe Recovery
- **Status:** Not Started
- **Architecture:** RFC 5109 / RFC 8627 Forward Error Correction, rapid reference recovery, audio PLC.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** Bandwidth overhead of FEC.
- **Commit Hash:** —

### 14. Direct D3D11VA Hardware Decoding on the Guest
- **Status:** Not Started
- **Architecture:** Zero-copy GPU surface presentation via DXGI swapchain without CPU roundtrip.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** D3D11 hardware decoder.
- **Known Limitations:** Windows 10/11 exclusive.
- **Commit Hash:** —

### 15. Host and Guest Statistics Overlay
- **Status:** Not Started
- **Architecture:** Non-intrusive interactive Slint HUD overlay displaying real-time telemetry, RTT, bitrate, loss, and hardware stats.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** Minimal GPU rasterizer overhead.
- **Commit Hash:** —

### 16. Virtual Gamepad Support for Each Guest
- **Status:** Not Started
- **Architecture:** Multi-controller virtual input feeder for up to 4 simultaneous guest gamepads with independent rumble.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Virtual HID / XInput device support.
- **Known Limitations:** Requires virtual bus driver installation or native HID injection.
- **Commit Hash:** —

### 17. Relative Mouse Mode and Clipboard Synchronization
- **Status:** Not Started
- **Architecture:** Confined relative mouse delta streaming with DPI normalization; permission-gated text clipboard sync.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Win32 Raw Input.
- **Known Limitations:** Text-only clipboard sync (files excluded).
- **Commit Hash:** —

### 18. Windows Graphics Capture Fallback
- **Status:** Not Started
- **Architecture:** Modern `Windows.Graphics.Capture` API fallback for multi-GPU, hybrid laptops, or non-primary displays.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Windows 10 build 19041+.
- **Known Limitations:** Yellow capture border on older Windows builds.
- **Commit Hash:** —

### 19. Reconnection and Host Crash/Sleep Recovery
- **Status:** Not Started
- **Architecture:** Exponential backoff connection recovery, session token re-validation, device sleep/wake handlers.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** Power state transitions take 2-5 seconds.
- **Commit Hash:** —

### 20. Signaling Server Hardening
- **Status:** Not Started
- **Architecture:** TLS/WSS, room token expiry, client rate-limiting, IP-based connection limits, structured audit logging.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Rust signaling host.
- **Known Limitations:** None.
- **Commit Hash:** —

### 21. Virtual Display Driver for Headless Hosts
- **Status:** Not Started
- **Architecture:** IddCx Indirect Display Driver integration for headless servers and custom virtual resolution generation.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Windows 10/11 WHQL driver signing or test-signing mode.
- **Known Limitations:** Requires driver installation.
- **Commit Hash:** —

### 22. Host Service, Tray Application and Wake-on-LAN
- **Status:** Not Started
- **Architecture:** Session 0 service manager, Session 1 interactive tray companion with WoL magic packet generation.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** Motherboard & NIC with WoL support.
- **Known Limitations:** WoL does not traverse external routers without port forwarding.
- **Commit Hash:** —

### 23. AV1, 10-Bit and HDR
- **Status:** Not Started
- **Architecture:** AV1 NVENC encoding, 10-bit P010 format, BT.2020 PQ color space capture and tone-mapping for SDR guests.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** RTX 40-series GPU (AV1 encode) and HDR monitor.
- **Known Limitations:** Requires latest GPU architectures.
- **Commit Hash:** —

### 24. Finish and Polish the Slint UI
- **Status:** Not Started
- **Architecture:** Complete Pixel Mint Material Design 3 UI wired to all backend capabilities with 0 mock controls.
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** None.
- **Known Limitations:** None.
- **Commit Hash:** —

### 25. Cross-Platform Guest Clients
- **Status:** Not Started
- **Architecture:** Port guest streaming client to macOS (VideoToolbox), Linux (VA-API), Android (MediaCodec), and Web (WebRTC).
- **Files Changed:** TBD
- **Tests Added:** TBD
- **Benchmark Evidence:** TBD
- **Hardware Requirements:** macOS, Linux, Android, Web target devices.
- **Known Limitations:** Operating system specific permission models.
- **Commit Hash:** —
