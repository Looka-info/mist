# Mist Platform — Field Testing & Validation Methodology

This document outlines the standard operational procedures, test scenarios, network profiles, metric definitions, and pass/fail criteria for field testing and qualifying the **Mist Native Game Streaming Platform**.

---

## 1. Objectives

Field testing evaluates Mist beyond idealized local unit tests by subjecting the native engine to realistic network impairments, physical audio loopback endpoints, varying display resolutions, remote input dispatching, and firewall traversal topologies.

Primary validation goals:
- Measure actual end-to-end presentation and dispatch latencies under varying bandwidth limits.
- Verify in-flight dynamic NVENC rate adaptation without pipeline stalls or dropped frames.
- Ensure audio and video synchronization remains within perceptual tolerance ($\le 40$ ms skew).
- Prevent remote input starvation or injection buffer overflow.
- Reconcile accounting counters across all endpoints to ensure zero unaccounted packet or frame loss.

---

## 2. Testbed Hardware & Topology

```
+--------------------------------------------------------------------------------+
|                             FIELD TEST TOPOLOGY                                |
|                                                                                |
|   +-----------------------+                    +---------------------------+   |
|   |       Host PC         |                    |         Client PC         |   |
|   | AMD Ryzen 5 5600      |                    | Direct3D 11 / NVDEC       |   |
|   | GTX 1660 SUPER        |                    | WASAPI Audio Playback     |   |
|   | NVENC H.264 Encoder   |                    | Win32/RawInput Capture    |   |
|   | WASAPI Loopback       |                    |                           |   |
|   +-----------+-----------+                    +-------------+-------------+   |
|               |                                              |                 |
|               | (Media: UDP/SRTP RTP)                        |                 |
|               +-----------------------\ /--------------------+                 |
|                                        X                                       |
|               +-----------------------/ \--------------------+                 |
|               | (Signaling: WS / JSON)                       |                 |
|               v                                              v                 |
|   +-----------------------+                    +---------------------------+   |
|   |  Signaling Service    |                    |    STUN / TURN Relay      |   |
|   |  Rust Tokio ws://     |                    |  Coturn / RFC 5766 Relay  |   |
|   +-----------------------+                    +---------------------------+   |
+--------------------------------------------------------------------------------+
```

### Reference Benchmark Host
- **CPU:** AMD Ryzen 5 5600 (6 Cores / 12 Threads @ 3.50 GHz).
- **GPU:** NVIDIA GeForce GTX 1660 SUPER (Turing TU116, NVENC 7th Gen).
- **Display:** 1920x1080 @ 60 Hz (DXGI Desktop Duplication).
- **Audio Endpoint:** 48,000 Hz, 2 Channels, 32-bit Float (WASAPI Loopback).
- **OS:** Windows 11 Pro 64-bit.

---

## 3. Standard Field Test Scenarios

### Scenario A: Pristine High-Speed LAN
- **Objective:** Establish baseline throughput, minimal encode/decode latency, and maximum visual fidelity.
- **Network Profile:** 1 Gbps Ethernet or Wi-Fi 6, RTT $< 2$ ms, Packet Loss $0.00\%$, Jitter $< 0.5$ ms.
- **Target Profile:** `Quality` (1080p60 @ 15,000 kbps CBR).
- **Pass Criteria:**
  - Video FPS $\ge 59.5$.
  - Decode efficiency = $100.0\%$ (0 queue discards).
  - Average bitrate $\ge 11.0$ Mbps.
  - A/V skew $< 25$ ms.
  - Input drop rate = $0.00\%$.

### Scenario B: Bandwidth Throttled WAN (10 Mbps & 5 Mbps)
- **Objective:** Verify Additive Increase / Multiplicative Decrease (AIMD) congestion control and in-flight NVENC reconfiguration.
- **Network Profile:** Downlink throttled to 10 Mbps and 5 Mbps via netem/traffic control.
- **Target Profile:** `Adaptive` (dynamic 2,000–20,000 kbps).
- **Pass Criteria:**
  - Controller throttles video bitrate within $\le 1,000$ ms of bottleneck detection.
  - Audio (128 kbps) and Input (50 kbps) channels maintain zero drops.
  - Zero decoder context resets or pipeline crashes.

### Scenario C: Packet Loss Bursts & Jitter (15% Loss)
- **Objective:** Evaluate resilience to packet loss bursts, RTCP feedback smoothing, and recovery backoff.
- **Network Profile:** 15% packet loss burst for 2,000 ms, 15 ms jitter.
- **Pass Criteria:**
  - State machine transitions cleanly to `Congested` or `CriticallyCongested`.
  - Immediate multiplicative backoff applied to video bitstream ($\ge 20\%$ reduction).
  - Pipeline remains functional without deadlocking.
  - Smooth recovery via Additive Increase upon link restoration.

### Scenario D: Symmetric NAT & TURN Relay Traversal
- **Objective:** Verify candidate gathering, ICE connectivity checks, and seamless media relay fallback when direct P2P is blocked.
- **Network Profile:** Symmetric NAT on both host and client endpoints with direct UDP blocked.
- **Command Configuration:**
  ```powershell
  # Host
  mist_host.exe --stream-live --turn-server "turn:turn.example.com:3478" --turn-username "test" --turn-password "secret"
  
  # Client
  mist_client.exe --stream-receive --turn-server "turn:turn.example.com:3478" --turn-username "test" --turn-password "secret"
  ```
- **Pass Criteria:**
  - Active candidate pair resolves to `ConnectionType::RelayedTurn`.
  - Media stream flows reliably through the relay without ICE timeouts.

---

## 4. Reconciled Telemetry Schema & Counter Definitions

Every field test produces machine-readable JSON reports from both Host and Client:

### Host Telemetry (`artifacts/host-*.json`)
- `frames_encoded`: Total video frames submitted to and output by NVENC.
- `frames_dropped`: Frames dropped at the capture or queue stage.
- `actual_fps`: Measured encoder throughput across the run.
- `bitrate_kbps`: Measured average video bitstream throughput.
- `packets_sent`: Total RTP transport packets sent across all tracks.
- `bytes_sent`: Total payload bytes transmitted over WebRTC.
- `audio_packets_sent`: Opus audio packets dispatched to the audio track.
- `input_packets_received`: SCTP input packets received from client.
- `input_events_processed`: Win32/virtual input events injected on host.
- `input_events_dropped`: Input events dropped due to permission denial or rate limits.

### Client Telemetry (`artifacts/client-*.json`)
- `frames_received`: Complete H.264 access units assembled from RTP.
- `frames_submitted`: Access units submitted to Microsoft MFT decoder.
- `frames_decoded`: Uncompressed NV12 surfaces output by MFT decoder.
- `frames_discarded`: Frames dropped from the internal bounded presentation queue.
- `decoder_errors`: Direct3D 11 or Media Foundation hardware decoding failures.
- `bitrate_mbps`: Measured incoming video bitstream bandwidth.
- `audio_packets_received`: Opus audio packets received over WebRTC.
- `audio_frames_decoded`: PCM audio samples decoded and sent to WASAPI.
- `audio_underruns`: Instances where the audio playback buffer starved.
- `av_skew_p95_ms`: 95th percentile presentation skew between audio and video timestamps.

---

## 5. Automated Field Qualification Execution

To run the automated 10-step qualification suite:
```powershell
powershell -ExecutionPolicy Bypass -File scripts\verify-milestone-7.ps1
```

This script automatically executes unit test suites, diagnostic CLI commands, desktop self-test, installer packaging, and a live 10-second tri-stream session, validating that all reconciled accounting checks pass before outputting `artifacts/field-test-report.json`.
