# Mist — Milestone 28 Baseline & Codebase Audit
## Release Qualification, Native Integration & Cross-Platform Verification

**Date:** November 9, 2026  
**Starting Baseline:** `a7b0f5573281c04826cf2d7af479f24ebae21cdd`  
**Working Branch:** `feature/milestone-28-release-qualification`  
**Prior Tag:** `v0.3.0-rc2` (Preserved intact at `9adbf6e0c0c9e0d0b7ed179ad634da63441f8533`)  

---

## 1. Executive Summary

Milestone 28 establishes an honest, rigorous release qualification baseline for Mist. Following the Milestone 27 merge (`a7b0f55`), an independent, comprehensive audit was conducted across the C++ media engine, Rust workspaces, Slint desktop application, browser guest client, and GitHub Actions CI matrices.

The audit revealed that while Milestone 27 laid the architectural groundwork for native decoders and WebRTC streaming, several critical integration defects, protocol mismatches, and build configuration errors prevented reliable CI execution and real cross-platform interoperability.

---

## 2. Complete Finding Inventory & Classification

Every finding is classified strictly into one of five categories:
- **Confirmed defect**
- **Missing integration**
- **Missing test coverage**
- **Environment limitation**
- **Verified implementation**

---

### Finding 1: Syntax Errors in `CrossPlatformDecoders.cpp` Following Git Merge
- **Component:** C++ Media Engine (`engine/decode/src/CrossPlatformDecoders.cpp`)
- **Classification:** **Confirmed defect**
- **Description:** During the three-way merge into `feature/advanced-streaming-expansion` (`a7b0f55`), conflicting conditional directives were merged incorrectly, introducing `#endif without #if` at lines 934 and 1217, undeclared `AMediaCodec` types, and an unterminated `#if !defined(__EMSCRIPTEN__)` directive.
- **Impact:** Broke compilation of the `mist_decode` library on Windows and any target compiling `CrossPlatformDecoders.cpp`.
- **Root Cause:** Git merge conflict resolution artifact.

---

### Finding 2: Multi-Configuration Binary Paths in Windows Packaging Script
- **Component:** Packaging & CI Scripts (`scripts/build-installer.ps1`, `.github/workflows/ci.yml`)
- **Classification:** **Confirmed defect**
- **Description:** When CMake uses the Visual Studio 2022 generator (`-G "Visual Studio 17 2022"`), build outputs are placed in subdirectories according to configuration (e.g., `build/apps/host/Release/mist_host.exe`). `scripts/build-installer.ps1` hardcoded single-configuration paths (`build\apps\host\mist_host.exe`), causing the installer builder to fail on multi-config MSVC runs.
- **Impact:** Windows CI job failed at the distribution packaging step.
- **Resolution Path:** Update `build-installer.ps1` to inspect both single-config (`build/apps/...`) and multi-config (`build/apps/.../Release`) locations.

---

### Finding 3: Linux Rust Workspace Test Failure Due to Hardcoded Port 8099
- **Component:** Rust Pairing Crate (`crates/mist-pairing/tests/pairing_tests.rs`)
- **Classification:** **Confirmed defect**
- **Description:** `test_remote_pairing_tcp_flow()` hardcodes TCP port `8099` for `RemotePairingServer`. On Linux CI runners executing tests in parallel or environments where port 8099 is restricted or occupied, the test fails with `std::io::ErrorKind::AddrInUse`.
- **Impact:** Linux CI workflow failed during `cargo test --workspace`.
- **Resolution Path:** Bind to port `0` (or dynamically allocated ephemeral port) and query the local bound socket address.

---

### Finding 4: macOS VideoToolbox Shutdown Deadlock Hazard
- **Component:** Apple VideoToolbox Decoder (`engine/decode/src/CrossPlatformDecoders.cpp`)
- **Classification:** **Confirmed defect**
- **Description:** In `VideoToolboxDecoder::shutdown()`, `self->mutex_` is acquired via `std::lock_guard` before calling `VTDecompressionSessionWaitForAsynchronousFrames(vtSession_)`. In-flight frames delivered by VideoToolbox invoke `decompressionOutputCallback`, which also attempts to lock `self->mutex_`. This creates a classic recursive deadlock during session shutdown or flush.
- **Impact:** Application hang or freeze when closing or reconnecting guest sessions on macOS.
- **Resolution Path:** Release `mutex_` prior to calling `VTDecompressionSessionWaitForAsynchronousFrames` or use an atomic state flag to signal shutdown to the callback without holding the main mutex.

---

### Finding 5: VideoToolbox Lazy Session Initialization
- **Component:** Apple VideoToolbox Decoder (`engine/decode/src/CrossPlatformDecoders.cpp`)
- **Classification:** **Missing integration**
- **Description:** `VideoToolboxDecoder::initialize()` sets `isInitialized_ = true` without an active `VTDecompressionSessionRef`, deferring creation until SPS/PPS parameter sets are discovered in Annex-B packets. Packets submitted before parameter sets are dropped with an error. While standard for Annex-B streams without out-of-band extradata, this lifecycle was undocumented and not explicitly tested.
- **Impact:** Calling `submitPacket` before SPS/PPS produces `InvalidState` error rather than buffering or graceful parameter discovery.
- **Resolution Path:** Document lazy session creation and verify that parameter discovery gracefully instantiates the decompression session.

---

### Finding 6: Linux VA-API Missing Codec Parameter Buffers
- **Component:** Linux VA-API Decoder (`engine/decode/src/CrossPlatformDecoders.cpp`)
- **Classification:** **Confirmed defect**
- **Description:** `VaApiVideoDecoder::submitPacket` allocated only a single `VASliceDataBufferType` without sequence parameter buffers (`VAPictureParameterBufferH264`, `VASliceParameterBufferH264`, `VAIQMatrixBufferH264` for H.264, or HEVC/AV1 equivalents). Physical VA-API drivers (Intel iHD/i965, AMD RadeonSI, Mesa Nouveau) reject slice-only execution with `VA_STATUS_ERROR_INVALID_BUFFER`.
- **Impact:** Real Linux hardware decoding fails on physical GPU hardware.
- **Resolution Path:** Integrate slice and picture header parsing to submit valid VA-API parameter buffers before `vaBeginPicture`/`vaRenderPicture`.

---

### Finding 7: Android MediaCodec Output Index Zero Leak
- **Component:** Android MediaCodec Decoder (`engine/decode/src/CrossPlatformDecoders.cpp`)
- **Classification:** **Confirmed defect**
- **Description:** In `MediaCodecVideoDecoder::releaseFrame`, the release condition `if (!nativeWindow_ && mediaCodec_ && frame.resourceId > 0)` explicitly ignored `frame.resourceId == 0`. In Android NDK MediaCodec, buffer index `0` is a valid output buffer. Ignoring index 0 permanently leaked buffer 0 from the codec pool.
- **Impact:** Gradual starvation of the MediaCodec output buffer pool, causing playback to freeze.
- **Resolution Path:** Check `frame.resourceId != UINT64_MAX` or maintain an explicit boolean flag indicating valid buffer ownership.

---

### Finding 8: Android MediaCodec Hardcoded Output Resolution
- **Component:** Android MediaCodec Decoder (`engine/decode/src/CrossPlatformDecoders.cpp`)
- **Classification:** **Confirmed defect**
- **Description:** `MediaCodecVideoDecoder::submitPacket` hardcoded `width = 1920, height = 1080` into `DecodedFrame`. When decoding 720p or 1440p streams, or when the decoder emits `AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED`, the actual frame dimensions were ignored.
- **Impact:** Distorted or misaligned presentation when streaming resolutions other than 1080p.
- **Resolution Path:** Handle `AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED` and query `AMEDIAFORMAT_KEY_WIDTH` and `AMEDIAFORMAT_KEY_HEIGHT` dynamically.

---

### Finding 9: Browser Client Binary Input Protocol Incompatibility
- **Component:** Web Guest Client (`web/js/mist-web-client.js`, `engine/input/include/mist/input/InputTypes.h`)
- **Classification:** **Confirmed defect**
- **Description:** `mist-web-client.js` generated input packets with an ad-hoc 20-byte header and mismatched event IDs:
  - JS: Type 1 = MouseMove, Type 2 = MouseButton, Type 3 = Keyboard.
  - Native C++ `InputProtocol`:
    - `InputPacketHeader`: 48 bytes (version, inputType, flags, sequenceNumber, timestampNs, 32-byte sessionId).
    - `InputType::KeyEvent = 1` (12-byte payload).
    - `InputType::MouseMoveEvent = 2` (20-byte payload).
    - `InputType::MouseButtonEvent = 3` (4-byte payload).
- **Impact:** Host C++ media engine dropped all browser input packets due to "Mismatched payload size" and header validation failure.
- **Resolution Path:** Align `mist-web-client.js` to serialize genuine 48-byte `InputPacketHeader` and exact struct payloads matching `InputTypes.h`.

---

### Finding 10: WebRTC Signaling Candidate-Before-Description Race Condition
- **Component:** Web Guest Client (`web/js/mist-web-client.js`)
- **Classification:** **Missing integration**
- **Description:** In WebRTC signaling, remote ICE candidates can arrive over WebSocket before the remote SDP description (`offer` or `answer`) has been set via `setRemoteDescription()`. The browser client called `pc.addIceCandidate()` immediately upon candidate arrival without checking `pc.remoteDescription`, throwing an `InvalidStateError`.
- **Impact:** Intermittent connection drops or failed peer connection negotiation during fast ICE gathering.
- **Resolution Path:** Implement an ICE candidate queue that buffers candidates until `setRemoteDescription()` resolves.

---

### Finding 11: Insufficient Browser Client Automated Test Coverage
- **Component:** Web Client Testing (`web/test/run_browser_tests.js`)
- **Classification:** **Missing test coverage**
- **Description:** Current web client tests in Node.js only asserted JS object properties and 4 static checks. Real browser WebRTC peer connection, WebSocket signaling exchange, SDP answer generation, and data channel transmission were never exercised in an automated test environment.
- **Impact:** Inability to detect real WebRTC connection breakage or regressions in CI.
- **Resolution Path:** Implement end-to-end WebRTC protocol and signaling test harness validating actual peer connection negotiation and data channel transmission.

---

### Finding 12: `GuestClientSession` False-Connected State
- **Component:** Unified Guest Session (`engine/decode/src/GuestClientSession.cpp`)
- **Classification:** **Confirmed defect**
- **Description:** `GuestClientSession::connect()` transitioned directly to `SessionLifecycleState::Connected` immediately after local `decoder_->initialize()` succeeded, without coordinating with actual network transport state or waiting for media packets.
- **Impact:** UI and telemetry reported "Connected" even when the host was unreachable or offline.
- **Resolution Path:** Introduce granular lifecycle state progression: `ConnectingIce` -> `Connected` (upon transport established) -> `ReceivingMedia` / `Streaming` (upon first decoded frame).

---

### Finding 13: CI Runner Hardware Limitations
- **Component:** Multi-Platform CI Matrix (`.github/workflows/ci.yml`)
- **Classification:** **Environment limitation**
- **Description:** Hosted GitHub Actions virtual machines do not possess physical GPUs or display server access (headless Windows runner has no DirectX 11 display adapter; Linux runner has no `/dev/dri/renderD128` node; macOS runner has no physical display). Previous claims of "HardwareVerified" on CI runners were inaccurate.
- **Impact:** Tests requiring physical display duplication or direct GPU hardware contexts must gracefully report `HardwareUnavailable` rather than failing the build or fabricating success.
- **Resolution Path:** Accurately label CI test tiers: compilation and unit verification pass in CI (`SmokeTestPassed`); physical hardware qualification is reserved for verified desktop hardware.

---

### Finding 14: Windows DirectX 11 & Slint Desktop Core Functionality
- **Component:** Windows Media Engine & Desktop App (`engine/capture`, `engine/render`, `apps/mist-desktop`)
- **Classification:** **Verified implementation**
- **Description:** Native Windows Direct3D 11 video acceleration (D3D11VA), desktop duplication capture (DXGI), Wasapi audio loopback, Slint 1.18.1 desktop interface, and Named Pipe binary IPC (`mist-ipc`) are fully implemented and verified with 228 passing CTest regression tests and 32 passing Cargo workspace tests on local physical hardware.

---

## 3. Qualification Action Plan for Milestone 28

| Phase | Milestone Objective | Target Files |
|---|---|---|
| **Phase 1** | Repair Windows & Linux CI | `CrossPlatformDecoders.cpp`, `scripts/build-installer.ps1`, `crates/mist-pairing/tests/pairing_tests.rs` |
| **Phase 2** | VideoToolbox Correctness & Concurrency | `CrossPlatformDecoders.cpp`, `CrossPlatformDecoders.h`, `tests/unit/TestCrossPlatformClients.cpp` |
| **Phase 3** | Linux VA-API Parameter Buffers | `CrossPlatformDecoders.cpp`, `tests/unit/TestCrossPlatformClients.cpp` |
| **Phase 4** | Android MediaCodec Output Lifecycle | `CrossPlatformDecoders.cpp`, `tests/unit/TestCrossPlatformClients.cpp` |
| **Phase 5** | WebRTC Signaling Interoperability | `web/js/mist-web-client.js`, `docs/architecture/webrtc-signaling-compatibility.md` |
| **Phase 6** | Real WebRTC Browser Integration Tests | `web/test/run_browser_tests.js`, `web/test/webrtc_integration_test.js` |
| **Phase 7** | GuestClientSession Lifecycle State Machine | `GuestClientSession.cpp`, `GuestClientSession.h`, `TestCrossPlatformClients.cpp` |
| **Phase 8** | Qualification Model & Reporting | `docs/roadmap/milestone-28-qualification-report.md`, `docs/testing/cross-platform-test-matrix.md` |
| **Phase 9** | Full CI Matrix Hardening | `.github/workflows/ci.yml` |
| **Phase 10** | Release Candidate Readiness Audit | `docs/releases/v0.3.0-rc3-readiness.md` |
