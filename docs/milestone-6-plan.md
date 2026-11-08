# Mist Milestone 6 Implementation Plan
**Adaptive Streaming, Network QoS, Device Pairing & Production Hardening**

**Author:** Mist Systems Architecture Team  
**Date:** November 8, 2026  
**Status:** In Progress  
**Target:** Milestone 6 Completion  

---

## 1. Executive Mission

Milestones 0 through 5 established a high-performance native Windows gaming-streaming foundation with synchronized 1080p60 H.264 video (NVENC/D3D11VA), 48 kHz Opus audio (WASAPI loopback/playback), and low-latency remote control (SendInput/XInput/DataChannels) over WebRTC.

Milestone 6 hardens the platform for real-world production deployment across four mission-critical workstreams:
1. **Network QoS & Adaptive Bitrate:** Real-time media-path RTP/RTCP telemetry, sender-side congestion estimation state machine, dynamic NVENC bitrate reconfiguration without tearing down encoder sessions, and multi-tier quality profiles.
2. **Network Resilience & STUN/TURN:** Authenticated TURN relay traversal, direct vs relayed ICE candidate pair identification, connection state monitoring, and keyframe/ICE recovery.
3. **Desktop Connection Experience & Secure Pairing:** Cryptographic device pairing codes, expiration lifecycle, host approval, trusted device store with granular permissions (video, audio, keyboard, mouse, gamepad), and native desktop application UI.
4. **Production Readiness & Distribution:** Dependency compatibility diagnostics (`--dependency-audit`), Windows release packaging, uninstaller, Authenticode code signing script, and release integrity manifests.

---

## 2. Architecture & Subsystem Layout

```
engine/transport/qos/
├── QosTypes.h                      (RTP/RTCP stats, CongestionState, QualityProfile, BandwidthBudget)
├── NetworkQualityEstimator.h/.cpp  (EWMA smoothing of RTT, jitter, loss, and available bandwidth)
├── CongestionController.h/.cpp     (State machine: Stable, Increasing, Congested, Recovering, Critical)
├── AdaptiveBitrateController.h/.cpp(AIMD adaptation, hysteresis, cooldown, NVENC reconfig caller)
└── QualityAdaptationManager.h/.cpp (Profiles: Performance, Balanced, Quality, Adaptive)

engine/security/
├── PairingManager.h/.cpp           (6-digit pairing codes, expiration, trusted devices JSON store)
└── ...

engine/transport/
├── WebRtcConfig.h / .cpp           (STUN/TURN URLs, credentials, relay type TurnUdp/TurnTcp/TurnTls)
├── PeerConnectionManager.h/.cpp    (Relay credentials, getSelectedCandidatePair -> Direct vs Relayed)
└── ...

engine/encode/
├── NvencVideoEncoder.h/.cpp        (Enhanced setBitrate() with rate control parameters and VBV tuning)
└── ...

apps/desktop/
├── include/DesktopApplication.h
└── src/DesktopApplication.cpp      (Interactive multi-screen navigation: Home, Pairing, Host, Settings, Live, Devices)

apps/host/ & apps/client/
├── Diagnostic commands: --dependency-audit, --system-info, --stream-capabilities, --network-info, --encoder-info

packaging/ & scripts/
├── scripts/build-installer.ps1     (Packages binaries, assets, configs, generates uninstaller)
├── scripts/sign-release.ps1        (Authenticode signing, timestamping, release-manifest.json SHA-256)
└── scripts/verify-milestone-6.ps1  (Comprehensive automated verification suite)
```

---

## 3. Workstream Execution Roadmap

### Phase 1: Network Telemetry & Congestion-Control Subsystem (`engine/transport/qos`)
- Define `QosTypes.h` with `RtpStatistics`, `RtcpStatistics`, `CongestionState`, `QualityProfile`, and `ProfileConfig`.
- Implement `NetworkQualityEstimator` with exponential moving averages for RTT, jitter, packet loss rate, and throughput.
- Implement `CongestionController` implementing conservative AIMD with hysteresis (1000ms cooldown) and isolated loss tolerance.
- Implement `AdaptiveBitrateController` translating network recommendations into safe NVENC bitrate adjustments.
- Implement `QualityAdaptationManager` enforcing budget division across video, audio (128 kbps), input (~50 kbps), and transport headroom.

### Phase 2: Encoder Dynamic Reconfiguration & Transport Integration
- Validate and enhance `NvencVideoEncoder::setBitrate` with rate control parameters, VBV buffer adjustments, and capability sanity bounds.
- Integrate `RembHandler` into `VideoTrackSender` to process incoming client REMB bitrates.
- Integrate `RtcpReceivingSession` into `VideoTrackReceiver` to generate receiver reports and REMB requests.
- Hook `AdaptiveBitrateController` into streaming loops in `apps/host`.

### Phase 3: STUN/TURN Relay Traversal & Network Resilience
- Enhance `PeerConnectionManager::initialize` to parse TURN credentials (`turn:` and `turns:` URLs, username, password, transport).
- Query active candidate pair via `pc_->getSelectedCandidatePair(&local, &remote)` and classify connection as `Direct` (Host/Srflx/Prflx) or `Relayed` (TURN).
- Add ICE restart and connection recovery handlers for transient network drops.

### Phase 4: Secure Device Pairing & Granular Permissions
- Implement `PairingManager` with 6-digit alphanumeric pairing codes (e.g. `MIST-829`), 5-minute expiration, and rate-limiting for failed attempts.
- Maintain trusted devices store (`trusted_devices.json`) recording device ID, friendly name, public key fingerprint, and permission masks.
- Separate permissions: video viewing, audio sharing, keyboard, mouse, gamepad.

### Phase 5: Native Desktop Connection Experience (`apps/desktop`)
- Build multi-screen desktop application architecture:
  - **Screen A (HOME):** Discovered hosts, connection status, quick connect.
  - **Screen B (PAIRING):** Pairing code input/generation, expiration timer, host approval.
  - **Screen C (HOST):** Host enablement toggle, active viewer info, permission toggles.
  - **Screen D (STREAM SETTINGS):** Resolution, FPS, Bitrate, Profile selection, VSync.
  - **Screen E (LIVE SESSION):** Real-time streaming metrics overlay (FPS, bitrate, RTT, jitter, loss, connection type).
  - **Screen F (DEVICE MANAGEMENT):** Trusted devices list, device rename, permission revoking.

### Phase 6: Production Packaging, Code Signing & Diagnostics
- Implement dependency compatibility audit commands on host and client.
- Create `scripts/build-installer.ps1` for clean installation, upgrade, and uninstallation.
- Create `scripts/sign-release.ps1` with Authenticode signtool / self-signed dev fallback, timestamping, and `release-manifest.json`.

### Phase 7: Verification, Benchmarking & Documentation
- Author unit tests for QoS, congestion controller, pairing manager, and installer metadata.
- Author `scripts/verify-milestone-6.ps1` and run full live streaming benchmark with adaptive bitrate changes.
- Author ADRs 0027 to 0034 and complete `docs/milestone-6-report.md`.
