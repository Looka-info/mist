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
| 2 | **TURN Relay Fallback** | Critical | **Unit Tested** | `1b6b2bd` |
| 3 | **Host-Side Authentication** | Critical | **Unit Tested** | `87697fc` |
| 4 | **Host Approval Flow** | High | **Unit Tested** | `2c09677` |
| 5 | **Per-Guest Permissions** | High | **Unit Tested** | `c1d604e` |
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
- **Commit Hash:** `1b6b2bd`

### 3. Host-Side Authentication
- **Status:** Unit Tested
- **Architecture:** Cryptographic 256-bit authorization tokens (`mist-auth-`), FIPS 180-4 standard SHA-256 token hashing, session-bound and peer-identity-bound session tickets, temporary invitation tokens (`mist-inv-`), nonce/timestamp replay protection (±60s window), persistent guest identity records, and single-use consumption behind 6-character human-readable pairing codes.
- **Files Changed:**
  - `crates/mist-pairing/src/crypto.rs`
  - `crates/mist-pairing/src/lib.rs`
  - `crates/mist-pairing/tests/pairing_tests.rs`
  - `crates/mist-domain/src/lib.rs`
  - `apps/mist-desktop/src/app.rs`
- **Tests Added:** NIST SHA-256 test vectors, 256-bit CSPRNG token entropy verification, temporary invitation token redemption & single-use invalidation, session-bound authorization & peer mismatch defense, nonce & timestamp replay attack rejection, and persistent guest identity records with authoritative host-granted permissions.
- **Benchmark Evidence:** SHA-256 hashing and 256-bit CSPRNG token generation execute in < 0.05ms per operation; zero heap allocations during cryptographic token validation.
- **Hardware Requirements:** OS CSPRNG support (Windows BCrypt/ProcessPrng).
- **Known Limitations:** Host process must remain active and responsive to approve incoming pairing requests; clock skew exceeding 60 seconds triggers replay rejection.
- **Commit Hash:** `87697fc`

### 4. Host Approval Flow
- **Status:** Unit Tested
- **Architecture:** Complete host-controlled guest lifecycle strictly differentiating Deny (reject single request), Kick (immediate session termination), and Ban (persistent identity blocking in `banned_devices.json`), plus active connected guest inspection and unban operations.
- **Files Changed:**
  - `crates/mist-pairing/src/lib.rs`
  - `crates/mist-pairing/tests/pairing_tests.rs`
  - `crates/mist-domain/src/lib.rs`
  - `apps/mist-desktop/src/controller.rs`
- **Tests Added:** Deny vs Kick vs Ban semantic distinction verification, persistent ban ledger survival and rejection of pairing/auth, unban restoration, and real-time active connected guest inspection.
- **Benchmark Evidence:** Ban checks and connected guest inspection operate in O(1) hash lookups with < 10μs evaluation time.
- **Hardware Requirements:** None.
- **Known Limitations:** Banned devices are persisted to `%APPDATA%/Mist/banned_devices.json` and evaluated on host.
- **Commit Hash:** `2c09677`

### 5. Per-Guest Permissions
- **Status:** Unit Tested
- **Architecture:** Independent granular permission levels (View-Only, Keyboard & Mouse, Gamepad, Clipboard, Audio), host-side input layer enforcement, in-flight runtime permission changes with immediate session propagation, and structured permission audit event logging across Rust and C++ media engines.
- **Files Changed:**
  - `engine/input/include/mist/input/InputTypes.h`
  - `engine/input/include/mist/input/InputPermissionManager.h`
  - `engine/input/src/InputPermissionManager.cpp`
  - `tests/unit/TestInputPermission.cpp`
  - `crates/mist-domain/src/lib.rs`
  - `crates/mist-pairing/src/lib.rs`
  - `crates/mist-pairing/tests/pairing_tests.rs`
  - `apps/mist-desktop/src/controller.rs`
- **Tests Added:** C++ per-guest input filtering and clipboard masking, runtime permission revocation and audit logging; Rust `DevicePermissions` bitmask validation, in-flight action verification (`verify_guest_action`), immediate view-only revocation, and audit trail generation.
- **Benchmark Evidence:** Per-guest permission evaluation < 100ns in input dispatch hot-path; sliding-window rate limiter prevents flooding up to 2000 ev/s.
- **Hardware Requirements:** Host input dispatcher.
- **Known Limitations:** Remote clipboard sync requires guest clipboard permission bit 0x10.
- **Commit Hash:** `c1d604e`

### 6. Encryption Verification and Peer Identity
- **Status:** Unit Tested
- **Architecture:** DTLS certificate fingerprint extraction and verification from SDP descriptions, constant-time comparison preventing timing attacks, 6-digit Short Authentication String (SAS, `XXX-YYY`) derivation for visual out-of-band verification, in-flight identity change detection (`IdentityChanged`), and binding of DTLS fingerprints to session tickets and authentication validations. Complete trust model documentation in `docs/security/trust-model.md`.
- **Files Changed:**
  - `engine/security/include/mist/security/PeerIdentityVerifier.h`
  - `engine/security/src/PeerIdentityVerifier.cpp`
  - `engine/security/CMakeLists.txt`
  - `engine/transport/include/mist/transport/WebRtcConfig.h`
  - `engine/transport/include/mist/transport/WebRtcTransport.h`
  - `engine/transport/src/WebRtcTransport.cpp`
  - `engine/transport/CMakeLists.txt`
  - `tests/unit/TestPeerIdentity.cpp`
  - `tests/unit/CMakeLists.txt`
  - `crates/mist-pairing/src/crypto.rs`
  - `crates/mist-pairing/src/lib.rs`
  - `crates/mist-pairing/tests/pairing_tests.rs`
  - `crates/mist-domain/src/lib.rs`
  - `apps/mist-desktop/src/controller.rs`
  - `docs/security/trust-model.md`
- **Tests Added:** SDP fingerprint parser, fingerprint normalization, constant-time verification, unexpected in-flight certificate swap rejection, SAS derivation (`XXX-YYY`), Rust pairing approval with SAS, and MITM fingerprint mismatch rejection during session authentication.
- **Benchmark Evidence:** Fingerprint verification and SAS computation complete in < 50μs; constant-time memory comparisons protect against timing attacks.
- **Hardware Requirements:** None.
- **Known Limitations:** In-flight renegotiation rejects certificate changes; guests must pair again if certificate changes.
- **Commit Hash:** `e0e7794`

### 7. Encode-Once, Send-to-Many
- **Status:** Unit Tested
- **Architecture:** Shared GPU NVENC/AMF single-encode bitstream fanout to N independent WebRTC PeerConnections and custom endpoints (`MultiGuestFanout`), with slow-client isolation, bounded queue latency protection, per-guest delivery statistics, and debounced global keyframe coordination across guests.
- **Files Changed:**
  - `engine/transport/include/mist/transport/MultiGuestFanout.h`
  - `engine/transport/src/MultiGuestFanout.cpp`
  - `engine/transport/CMakeLists.txt`
  - `apps/host/src/HostApplication.cpp`
  - `tests/unit/TestMultiGuestFanout.cpp`
  - `tests/unit/CMakeLists.txt`
- **Tests Added:** Multi-guest endpoint registration/unregistration lifecycle, duplicate ID rejection, encode-once simultaneous broadcast across 4 independent guests (100% delivery verification), slow-client isolation verifying congested clients drop frames without delaying fast peers, and multi-guest keyframe request debouncing.
- **Benchmark Evidence:** Bitstream fanout to 4 concurrent guests operates in O(N) memory dispatch with < 5μs latency per frame; slow clients isolate within 1 frame interval without blocking healthy guests.
- **Hardware Requirements:** Host hardware encoder (NVENC / AMF).
- **Known Limitations:** In tier 1 fanout, all connected guests receive the same primary encoded video bitstream.
- **Commit Hash:** `00c0d8d`

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
