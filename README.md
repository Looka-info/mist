# Mist — High-Performance Native Windows Game Streaming

Mist is a lightweight, ultra-low-latency native Windows game-streaming application engineered for interactive remote gaming. Built entirely in C++20 and Rust without heavy web or Electron runtimes, Mist combines Direct3D 11 GPU capture, NVIDIA NVENC hardware video encoding, WASAPI audio loopback, sub-millisecond input injection, and native WebRTC peer-to-peer transport into an integrated, production-ready desktop experience.

---

## 1. Product Vision

Mist delivers a high-fidelity, Parsec-grade remote gaming experience on native Windows hardware. The project prioritizes:
- **Zero Framework Overhead:** Pure C++20 and native Win32/Direct3D 11 without WebViews, Node.js, or Electron shells.
- **Hardware-First Graphics:** Direct GPU VRAM capture via DXGI Desktop Duplication, direct-to-encoder NV12 conversion via hardware VideoProcessors, and zero-copy Direct3D 11 presentation.
- **Media-Path Adaptation:** Transport-driven network telemetry and in-flight NVENC dynamic bitrate adjustments without encoder restarts or dropped frames.
- **Operator-Controlled Security:** Ephemeral Base32 pairing codes, single-use token consumption, granular capability permissions, and immediate physical operator overrides.

---

## 2. Core Capabilities

- **Direct3D 11 GPU Desktop Capture:** 60 FPS, 1080p desktop acquisition utilizing the DXGI Desktop Duplication API with sub-millisecond frame acquisition.
- **NVIDIA NVENC Hardware Video Encoding:** Sub-2ms H.264 elementary bitstream compression with CBR rate control and in-flight dynamic bitrate adaptation.
- **Hardware-Accelerated Video Decoding:** Direct3D 11 / Microsoft Media Foundation H.264 Transform (NVDEC) with an exhaustive frame drain loop ensuring 100.0% decode efficiency.
- **WASAPI Audio Capture & Playout:** Low-latency loopback capture of Windows default audio endpoints, Opus audio encoding (48 kHz stereo, 128 kbps, restricted low-delay), and WASAPI playback with under 20 ms audio-video presentation skew.
- **Low-Latency Remote Input Control:** Win32 RawInput keyboard and relative mouse capture with sub-millisecond dispatching, coupled with client XInput gamepad polling and host virtual controller support.
- **Adaptive Network QoS & Congestion Control:** Native WebRTC RTCP receiver reports and REMB feedback driving an Additive Increase / Multiplicative Decrease (AIMD) state machine.
- **Cryptographic Device Pairing:** 6-character unambiguous Base32 pairing codes, 5-attempt rate-limiting lockout, host approval workflow, and atomic JSON persistence.
- **Native Desktop Shell:** Lean, responsive Win32/Direct3D console management interface (`mist_desktop.exe`) with six operational views.

---

## 3. Architecture Overview

```
+-----------------------------------------------------------------------------------------+
|                                        MIST HOST                                        |
|                                                                                         |
|  +--------------------+     +---------------------+     +----------------------------+  |
|  |   DXGI / Desktop   |---->|  D3D11 VideoProc    |---->|     NVIDIA NVENC API       |  |
|  |    Duplication     |     |   BGRA -> NV12      |     | Hardware H.264 CBR Encoder |  |
|  +--------------------+     +---------------------+     +----------------------------+  |
|                                                                        |                |
|  +--------------------+     +---------------------+                    v                |
|  |   WASAPI Loopback  |---->|   Opus Audio        |----->+---------------------------+  |
|  | 48 kHz Stereo Float|     | 48 kHz / 128 kbps   |      |  WebRTC Transport Sender  |  |
|  +--------------------+     +---------------------+      | (RFC 6184 H.264 / RFC 7587|  |
|                                                          |  Opus / SCTP DataChannels)|  |
|  +--------------------+     +---------------------+      +---------------------------+  |
|  | Windows Injection  |<----|  Input Dispatcher   |<-------------------+  |             |
|  | SendInput / ViGEm  |     |  Worker Queue (64)  |                       |             |
|  +--------------------+     +---------------------+                       |             |
+---------------------------------------------------------------------------|-------------+
                                                                            | Network
                                      WebRTC UDP (SRTP / DTLS 1.2)          |
+---------------------------------------------------------------------------|-------------+
|                                                                           v             |
|  +--------------------+     +---------------------+      +---------------------------+  |
|  | Direct3D 11 Window |<----| Microsoft Media     |<-----| WebRTC Transport Receiver |  |
|  | Headless / SwapChn |     | Foundation H.264 MFT|      | Complete AU Reassembly    |  |
|  +--------------------+     +---------------------+      +---------------------------+  |
|                                                                           |             |
|  +--------------------+     +---------------------+                       |             |
|  | WASAPI Playback    |<----|   Opus Audio        |<----------------------+             |
|  | Low-Latency Buffer |     | 48 kHz PCM Playout  |                       |             |
|  +--------------------+     +---------------------+                       |             |
|                                                                           |             |
|  +--------------------+     +---------------------+                       |             |
|  | Local Input Hook   |---->| Client Input Packet |-----------------------+             |
|  | RawInput / XInput  |     | Binary Protocol v1  |                                     |
|  +--------------------+     +---------------------+                                     |
|                                       MIST CLIENT                                       |
+-----------------------------------------------------------------------------------------+
```

---

## 4. Technology Stack

- **Core Engine:** C++20 (`mist_common`, `mist_capture`, `mist_encode`, `mist_decode`, `mist_render`, `mist_audio`, `mist_input`, `mist_session`, `mist_transport`, `mist_security`, `mist_telemetry`).
- **Graphics & Video:** DirectX 11.1, DXGI 1.2, NVIDIA Video Codec SDK (NVENC API v13.1), Microsoft Media Foundation.
- **Audio Codec:** WASAPI, reference `xiph/opus` v1.5.2 (Restricted Low-Delay mode, 10 ms framing).
- **Transport & Security:** `libdatachannel` v0.21, MbedTLS v3.6.2 (statically compiled DTLS-SRTP), RFC 6184 H.264 RTP, RFC 7587 Opus RTP, RFC 8831 SCTP DataChannels.
- **Signaling Service:** Rust 1.99, Tokio, Tungstenite, Serde (`mist-signaling.exe`).
- **Build System:** CMake 3.25+, Ninja, Clang++ (LLVM-MinGW UCRT x86_64).
- **Testing:** GoogleTest (114 automated unit tests).

---

## 5. Platform Requirements

### Host Machine (Streaming Server)
- **Operating System:** Windows 11 64-bit (22H2–24H2 recommended) or Windows 10 64-bit (21H2+).
- **Processor:** 6-core x86_64 CPU with AVX2 support (AMD Ryzen 5 3600+ or Intel Core i5-10400+).
- **Graphics Card:** NVIDIA GeForce GTX 10-series or newer (Pascal, Turing, Ampere, Ada Lovelace) with NVENC hardware support.
- **Graphics Driver:** NVIDIA Windows Display Driver $\ge 522.25$ (Verified on Driver `617.14`).
- **Display Session:** Active Windows graphical session attached to a physical monitor or virtual display adapter.
- **Audio Endpoint:** Windows default audio rendering endpoint operating in shared mode.

### Client Machine (Viewer)
- **Operating System:** Windows 10 or Windows 11 64-bit.
- **Graphics Card:** Direct3D 11 Feature Level 11_0 compatible GPU with H.264 hardware decoding (NVIDIA NVDEC, AMD AMF, Intel QuickSync, or CPU fallback).
- **Peripherals:** Keyboard, mouse, and/or XInput-compatible controller.

---

## 6. Subsystem Details

### 6.1 Video Pipeline
- **Capture:** `engine/capture/src/DxgiDesktopCapture.cpp` acquires DXGI duplicate frames directly in GPU VRAM without host CPU readback.
- **Format Conversion:** `D3D11VideoProcessor` executes GPU-side BGRA-to-NV12 color space conversion.
- **Encoder:** `engine/encode/src/NvencVideoEncoder.cpp` uses NVIDIA NVENC API v13.1. Supports in-flight CBR bitrate updates via `nvEncReconfigureEncoder`.
- **Decoder:** `engine/decode/src/D3D11VideoDecoder.cpp` uses Media Foundation Hardware MFT with an exhaustive drain loop ensuring zero frame loss.

### 6.2 Audio Pipeline
- **Capture:** `engine/audio/src/WasapiAudioCapture.cpp` captures audio in `AUDCLNT_STREAMFLAGS_LOOPBACK` shared mode at 48 kHz stereo.
- **Encoding:** `engine/audio/src/OpusAudioEncoder.cpp` encodes 10 ms (480-sample) audio blocks at 128 kbps.
- **Playout:** `engine/audio/src/WasapiAudioPlayback.cpp` feeds a low-latency playout queue, maintaining $\le 20$ ms audio-video presentation skew.

### 6.3 Input Pipeline
- **Capture:** Client hooks keyboard and mouse via Win32 RawInput and polls XInput controllers 0–3.
- **Protocol:** Serialized into 48-byte binary packets (`Binary Protocol v1`) with monotonic sequence tracking.
- **Injection:** Host `WindowsKeyboardInjector` and `WindowsMouseInjector` execute sanitized events via `SendInput`. Virtual controller support uses the ViGEmBus kernel driver with automatic user-mode fallback.
- **Operator Override:** Host physical mouse or keyboard activity instantly takes precedence over remote input.

### 6.4 Network QoS & Adaptive Streaming
- **Telemetry:** Ingests media-path RTCP Receiver Reports and REMB bandwidth estimates.
- **Smoothing:** Exponentially Weighted Moving Average (EWMA) smoothing for packet loss ($\alpha = 0.20$), RTT ($\alpha = 0.15$), and jitter.
- **AIMD Controller:** Operates 5 congestion states (`Stable`, `Increasing`, `Congested`, `CriticallyCongested`, `Recovering`).
- **Budgeting:** Strictly guarantees 50 kbps for inputs and 128 kbps for audio before dynamically allocating remaining bandwidth to video.

### 6.5 Pairing & Security
- **Pairing Codes:** 6-character codes drawn from 32 unambiguous characters (`23456789ABCDEFGHJKLMNPQRSTUVWXYZ`, ~30 bits entropy).
- **Brute-Force Lockout:** 5 consecutive failures trigger an automatic 60-second lockout.
- **Single-Use Consumption:** Pairing codes are immediately erased upon first verification.
- **Atomic Storage:** Trusted device records persist via temporary file write and atomic `MoveFileExA` rename.
- **Default-Deny:** Remote input permissions default to disabled until explicitly granted by the host operator.

---

## 7. Building from Source

### Prerequisites
1. **Windows 10/11 x64** with Developer Mode enabled.
2. **Clang++ (LLVM-MinGW UCRT)** or MSVC 2022 (v143 toolset).
3. **CMake 3.25+** and **Ninja 1.11+**.
4. **Rust 1.75+** (`cargo` and `rustc` for signaling service).
5. **NVIDIA Video Codec SDK** headers placed in `D:/tools/nv_codec_sdk/include` or configured via `NVCODEC_INCLUDE_DIR`.

### Compilation Steps
```powershell
# 1. Clone the repository
git clone https://github.com/mist/mist.git
cd mist

# 2. Configure CMake with Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. Compile native C++ binaries
cmake --build build --config Release

# 4. Build signaling service
cargo build --release --manifest-path services/signaling/Cargo.toml
Copy-Item services/signaling/target/release/mist-signaling.exe bin/mist-signaling.exe -Force
```

### Running the Test Suite
```powershell
./build/tests/unit/mist_unit_tests.exe
```

---

## 8. Installation & Packaging

### Automated Release Packaging
```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-installer.ps1
```
This generates the standalone distribution archive `dist/mist-v0.2.0-rc1-windows-x64.zip` (4.44 MB) and verifies the automated install/uninstall lifecycle.

### User Installation
1. Extract `mist-v0.2.0-rc1-windows-x64.zip`.
2. Run PowerShell as standard user:
   ```powershell
   powershell.exe -ExecutionPolicy Bypass -File .\install.ps1
   ```
3. Launch **Mist Desktop** from the Windows Start Menu.

---

## 9. Command-Line Diagnostics & Usage

### Host CLI Options (`mist_host.exe`)
```powershell
# System and diagnostic audits
mist_host.exe --system-info
mist_host.exe --stream-capabilities
mist_host.exe --network-info
mist_host.exe --encoder-info
mist_host.exe --dependency-audit

# Stream desktop capture over WebRTC
mist_host.exe --stream-live --audio --allow-remote-input `
    --signaling-url ws://127.0.0.1:8080 --session-id mist-live `
    --turn-server "turn:relay.example.com:3478" --turn-username "user" --turn-password "pass"

# Stream synthetic moving benchmark pattern (Headless/CI)
mist_host.exe --stream-test --audio --allow-remote-input `
    --duration 10 --report artifacts/host-report.json
```

### Client CLI Options (`mist_client.exe`)
```powershell
# System and decoder diagnostics
mist_client.exe --system-info
mist_client.exe --decoder-info
mist_client.exe --network-info

# Connect and receive stream
mist_client.exe --stream-receive --audio --enable-input `
    --signaling-url ws://127.0.0.1:8080 --session-id mist-live `
    --turn-server "turn:relay.example.com:3478" --turn-username "user" --turn-password "pass"

# Receive stream in headless benchmark mode
mist_client.exe --receive-test --audio --enable-input `
    --duration 10 --headless --report artifacts/client-report.json
```

### Desktop Management Shell (`mist_desktop.exe`)
```powershell
# Run interactive desktop management shell
mist_desktop.exe --ui

# Run automated desktop self-test
mist_desktop.exe --self-test
```

---

## 10. Benchmark & Field Qualification Summary

Verified results from the physical benchmark workstation (AMD Ryzen 5 5600, NVIDIA GTX 1660 SUPER, Windows 11 Pro):

| Pipeline Stage | Metric | Measured Result | Status |
|---|---|---|---|
| **Video Encoder** | Throughput / FPS | 602 frames @ 59.90 FPS | **Verified** |
| **Video Bitrate** | Average Encoded / Received | 11.53 Mbps / 10.87 Mbps | **Verified** |
| **Video Decoder** | Complete Access Units Received | 563 frames | **Verified** |
| **Video Decoder** | Decoded Surfaces / Efficiency | 563 frames (100.0% efficiency) | **Verified** |
| **Video Queue** | Intentional Queue Discards | 0 frames | **Verified** |
| **Video Decoder** | Hardware MFT Errors | 0 errors | **Verified** |
| **Audio Playout** | Playout Rate / Samples Rendered | 1,498 packets / 719,040 PCM samples | **Verified** |
| **A/V Sync Skew** | 95th Percentile Skew | 17.50 ms ($\le 40$ ms limit) | **Verified** |
| **Remote Input** | Dispatch Latency / Drop Rate | $< 0.35$ ms / 0.00% drops | **Verified** |
| **Unit Tests** | GoogleTest Suite Pass Rate | 112 passed, 2 skipped, 0 failed | **Verified** |

---

## 11. Release Information (`v0.2.0-rc1`)

- **Release Tag:** `v0.2.0-rc1`
- **Distribution Package:** `dist/mist-v0.2.0-rc1-windows-x64.zip` (4.44 MB)
- **Archive SHA-256:** `1FC45EC9DA5F0CACC7F6B473326A71BF7C2D3F0B352BDE41434889F05FD9C915`
- **Release Manifest:** `dist/mist-windows-x64/release-manifest.json`
- **Code Signing:** Authenticode signed using development certificate (`CN=Mist Development Release Signing`).

---

## 12. Known Limitations

1. **Development Code Signing:** Executables are signed with a development Authenticode certificate. Running on external Windows PCs will trigger a Microsoft Defender SmartScreen prompt until signed by a trusted public root CA.
2. **Display Session Attachment:** DXGI Desktop Duplication requires an active Windows graphical console session attached to a display adapter. Headless RDP sessions without GPU acceleration are not supported.
3. **Multi-Hour Endurance:** Benchmarks reflect verified 10.0-second sustained runs. Multi-hour endurance testing (30–60 minutes) has not been continuously monitored.
4. **Commercial TURN Field Testing:** Command-line options and candidate gathering for TURN are implemented and unit-tested, but live streaming over commercial public TURN relays across intercontinental WAN has not been qualified.
5. **Host GPU Vendors:** Host hardware encoding is currently limited to NVIDIA NVENC. AMD AMF and Intel QuickSync host encoders are planned for future milestones.

---

## 13. Project Roadmap

- **Milestones 0–7 (Complete):** Foundation, DXGI Capture, NVENC Encoding, D3D11 Decoding, WebRTC Transport, WASAPI Audio, Remote Input Control, Adaptive QoS, Production Hardening, and Release Candidate Qualification (`v0.2.0-rc1`).
- **Milestone 8 (Planned):** AMD AMF and Intel QuickSync host hardware encoding backends.
- **Milestone 9 (Planned):** Cross-platform client support (Linux/macOS using Vulkan and Metal presentation).
- **Milestone 10 (Planned):** Enterprise multi-tenant signaling and session management with public CA Authenticode distribution.

---

## 14. License & Attribution

Mist is a proprietary, high-performance native Windows game-streaming platform. Third-party libraries (`libdatachannel`, `mbedtls`, `opus`, `googletest`) are utilized under their respective open-source licenses.
