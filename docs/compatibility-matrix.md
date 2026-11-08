# Mist Platform — Hardware & Platform Compatibility Matrix

This document provides the authoritative hardware, driver, operating system, and runtime compatibility matrix for the **Mist Native Game Streaming Platform** as of Release Candidate **v0.2.0-rc1**.

---

## 1. Classification Categories

To ensure absolute engineering clarity, all matrix items are strictly classified into one of three statuses:

1. **Verified:** Confirmed through direct physical execution and recorded telemetry on the reference testbed.
2. **Expected but not independently tested:** Supported by architecture, APIs, or official vendor specifications, but not directly tested on a physical testbed in this qualification cycle.
3. **Unsupported or not verified:** Incompatible, outside MVP scope, or awaiting future engine adapters.

---

## 2. Operating System Compatibility

| Operating System | Architecture | Status | Engineering Details & Constraints |
|---|---|---|---|
| **Windows 11 Pro 64-bit (Version 24H2 / 23H2)** | x86_64 | **Verified** | Reference testbed OS (Build `26100.3194`). Native D3D11.1, DXGI Desktop Duplication, and WASAPI loopback verified. |
| **Windows 11 Home / Enterprise 64-bit** | x86_64 | **Expected but not independently tested** | Shares identical Win32 kernel and Direct3D 11.1 graphics subsystems. |
| **Windows 10 64-bit (Version 22H2 / 21H2)** | x86_64 | **Expected but not independently tested** | Target API level compatible. Requires Media Feature Pack on Windows 10 N editions. |
| **Windows Server 2022 / 2025** | x86_64 | **Expected but not independently tested** | Requires Desktop Experience pack, graphics vendor driver, and Windows Audio service enabled. |
| **Windows on ARM (ARM64)** | ARM64 | **Unsupported or not verified** | Native x86_64 binaries; ARM64 compilation and emulation layers not qualified. |
| **Windows 7 / 8 / 8.1** | Any | **Unsupported or not verified** | End-of-life. Lacks DXGI Desktop Duplication extensions and modern WASAPI loopback APIs. |
| **Linux / macOS / Android** | Any | **Unsupported or not verified** | Non-Windows platforms. Future cross-platform clients are planned for post-v0.3 milestones. |

---

## 3. Host Processor (CPU) Compatibility

| CPU Family | Configuration | Status | Notes |
|---|---|---|---|
| **AMD Ryzen 5 5600** (Zen 3) | 6 Cores / 12 Threads @ 3.50 GHz | **Verified** | Reference benchmark machine. Sub-millisecond input dispatch and low CPU overhead verified. |
| **AMD Ryzen 3000 / 5000 / 7000 / 9000 Series** | Zen 2 / 3 / 4 / 5 | **Expected but not independently tested** | Standard x86_64 AVX2 instruction set architecture. |
| **Intel Core 10th through 14th Gen / Core Ultra** | 4 to 24 Cores | **Expected but not independently tested** | Standard Win32 threading and WASAPI host compatibility. |
| **Legacy CPUs without AVX2** | Pre-2013 Architectures | **Unsupported or not verified** | Opus audio and SIMD color conversion paths assume AVX2 / SSE4.1 support. |

---

## 4. GPU Hardware Encoding (Host)

| GPU Model & Family | Architecture | NVENC Gen | Status | Performance & Notes |
|---|---|---|---|---|
| **NVIDIA GeForce GTX 1660 SUPER** | Turing (TU116) | 7th Gen | **Verified** | Reference benchmark GPU. 1080p60 H.264 encode latency $\le 1.82$ ms. Dynamic in-flight reconfiguration verified. |
| **NVIDIA GeForce RTX 2060 / 2070 / 2080** | Turing (TU106/104) | 7th Gen | **Expected but not independently tested** | Identical NVENC SIP core as TU116 with higher VRAM bandwidth. |
| **NVIDIA GeForce RTX 30-Series (Ampere)** | Ampere (GA106+) | 7th Gen | **Expected but not independently tested** | Supported by NVENC API v13.1. Dual-NVENC on high-end SKUs. |
| **NVIDIA GeForce RTX 40-Series (Ada Lovelace)**| Ada (AD106+) | 8th Gen | **Expected but not independently tested** | 8th gen NVENC; AV1 encoding scheduled for post-v0.3. |
| **NVIDIA GeForce GTX 10-Series (Pascal)** | Pascal (GP106+) | 5th Gen | **Expected but not independently tested** | Requires NVIDIA Driver $\ge 522.25$. Limited B-frame rate control. |
| **AMD Radeon (RX 5000 / 6000 / 7000)** | RDNA 1 / 2 / 3 | AMF | **Unsupported or not verified** | Host AMF encoder adapter boundary defined, but AMF backend is not implemented in MVP. |
| **Intel Arc Graphics (A-Series / Battlemage)** | Alchemist / Xe | QuickSync | **Unsupported or not verified** | Intel QuickSync host encoder not implemented in MVP. |

---

## 5. Client Hardware Decoding (Viewer)

| Decoder Backend | Underlying API | Status | Notes |
|---|---|---|---|
| **Microsoft Media Foundation H.264 MFT (NVDEC)** | Direct3D 11 / DXVA2 | **Verified** | Reference client pipeline. Hardware NV12 decoding $\le 2.45$ ms; 0 queue drops with drain loop. |
| **Microsoft Media Foundation H.264 MFT (AMD AMF)** | Direct3D 11 / DXVA2 | **Expected but not independently tested** | Windows standard MFT hardware pipeline on AMD Radeon GPUs. |
| **Microsoft Media Foundation H.264 MFT (Intel QSV)**| Direct3D 11 / DXVA2 | **Expected but not independently tested** | Windows standard MFT hardware pipeline on Intel Iris Xe and Arc GPUs. |
| **Microsoft Media Foundation Software Fallback** | CPU Software MFT | **Expected but not independently tested** | Automatic fallback if GPU decoder context fails to initialize. |

---

## 6. Runtime, Driver & Graphics Requirements

| Dependency | Minimum Version | Verified Reference | Status |
|---|---|---|---|
| **NVIDIA Windows Display Driver** | $\ge 522.25$ | `32.0.16.1714` (Driver `617.14`) | **Verified** |
| **NVIDIA NVENC API** | API Version 11.0 | API Version 13.1 (`nvEncodeAPI64.dll`) | **Verified** |
| **Direct3D Graphics Runtime** | Direct3D 11.0 Feature Level 11_0 | Direct3D 11.1 Feature Level 11_1 | **Verified** |
| **Media Foundation Runtime** | Windows Media Foundation | MF Plat / MF ReadWrite (`mfplat.dll`) | **Verified** |
| **C++ Runtime Libraries** | UCRT (Universal C Runtime) | Windows 11 UCRT (MSVCRT / LLVM-MinGW) | **Verified** |

---

## 7. Audio Endpoints & Formats

| Audio Configuration | Host Capture Mode | Status | Notes |
|---|---|---|---|
| **48,000 Hz, 2 Channels, IEEE Float32** | WASAPI Loopback (Shared) | **Verified** | Reference endpoint (Speakers AULA-G7Pro). 10 ms framing, zero conversion overhead. |
| **44,100 Hz, 2 Channels, 16-bit PCM** | WASAPI Loopback (Shared) | **Expected but not independently tested** | Internal linear resampler converts to 48 kHz for Opus codec. |
| **5.1 / 7.1 Multi-Channel Surround** | WASAPI Loopback (Shared) | **Expected but not independently tested** | ITU-R BS.775 downmix matrix downmixes to stereo. |
| **Exclusive Mode Audio Endpoints** | WASAPI Exclusive | **Unsupported or not verified** | Windows prohibits loopback capture on exclusive-mode endpoints. Shared mode is required. |

---

## 8. Remote Input & Gamepad Injection

| Input Category | Capture / Injection API | Status | Notes |
|---|---|---|---|
| **Physical Keyboard** | Win32 RawInput $\rightarrow$ `SendInput` | **Verified** | Hardware scan codes; local operator physical override verified. |
| **High-Precision Gaming Mouse** | Win32 RawInput $\rightarrow$ `SendInput` | **Verified** | Sub-millisecond relative motion (`MOUSEEVENTF_MOVE`). |
| **Physical Gamepad (Client)** | Direct XInput Polling (`XInputGetState`)| **Verified** | Standard Xbox 360 / Xbox One controllers (indices 0–3). |
| **Virtual Gamepad Injection (Host)** | ViGEmBus Kernel Driver | **Expected but not independently tested** | Driver was not installed on benchmark host; verified via user-mode fallback. |
| **Virtual Gamepad Fallback (Host)** | User-Mode State Simulation | **Verified** | Memory-mapped state tracker activates automatically when ViGEmBus driver is absent. |

---

## 9. Network & Firewall Traversal

| Network Topology | Media Path | Candidate Type | Status |
|---|---|---|---|
| **Direct Local LAN (Subnet)** | Peer-to-Peer | `host` | **Verified** (Zero loss, $< 1$ ms latency). |
| **Direct Public WAN (STUN)** | Peer-to-Peer | `srflx` (STUN UDP 3478) | **Verified** (Trickle ICE candidate gathering). |
| **Symmetric / Enterprise NAT (TURN)** | Relayed Proxy | `relay` (TURN UDP/TCP/TLS) | **Expected but not independently tested** (CLI options and parsing verified; live link used direct candidate pair). |
