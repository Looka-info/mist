# Mist Platform — Hardware & Platform Compatibility Matrix

This matrix documents the hardware, driver, operating system, display, audio, and peripheral compatibility status for the **Mist Native Game Streaming Platform** as of Release Candidate **v0.2.0-rc1**.

---

## 1. Operating System Compatibility

| OS Version | Architecture | Host Support | Client Support | Notes |
|---|---|---|---|---|
| **Windows 11 24H2 / 23H2 / 22H2** | x86_64 | **Verified Primary** | **Verified Primary** | Reference testbed OS; full Direct3D 11.1 and WASAPI support. |
| **Windows 10 22H2 / 21H2** | x86_64 | **Supported** | **Supported** | Compatible; requires Media Foundation feature pack on N editions. |
| **Windows Server 2022 / 2025** | x86_64 | **Compatible** | **Compatible** | Requires Desktop Experience pack, GPU drivers, and audio service enabled. |
| **Windows 7 / 8 / 8.1** | Any | **Unsupported** | **Unsupported** | End-of-life; lacks Direct3D 11.1 and DXGI Duplication API extensions. |
| **Linux / macOS / Android** | Any | **Out of Scope** | **Planned (v0.4+)** | Future client ports will utilize engine adapter architecture. |

---

## 2. GPU Hardware & Video Codec Compatibility

### 2.1 Host Hardware Encoding (Capture & NVENC)

| GPU Family | Architecture | Codec | Status | Capabilities & Constraints |
|---|---|---|---|---|
| **NVIDIA Turing** (e.g. GTX 1660 SUPER, RTX 2060+) | TU116 / TU106 | H.264 (NVENC) | **Verified Reference** | Sub-2ms encode, in-flight reconfiguration, B-frames supported. |
| **NVIDIA Ampere** (e.g. RTX 3060, 3070, 3080) | GA106 / GA104 | H.264 (NVENC) | **Supported** | Dual-NVENC on high-end SKUs; improved rate control. |
| **NVIDIA Ada Lovelace** (e.g. RTX 4060, 4070+) | AD106 / AD104 | H.264 (NVENC) | **Supported** | 8th gen NVENC; AV1 supported in future milestones. |
| **NVIDIA Pascal** (e.g. GTX 1060, 1070, 1080) | GP106 / GP104 | H.264 (NVENC) | **Supported** | 5th gen NVENC; requires Driver $\ge 522.25$. |
| **AMD Radeon** (RX 5000 / 6000 / 7000) | RDNA 1/2/3 | H.264 (AMF) | **Planned (v0.3)** | Adapter boundary defined; AMF backend not in MVP. |
| **Intel Arc Graphics** (A380, A750, A770) | Alchemist / Xe | H.264 (QSV) | **Planned (v0.3)** | OneVPL / QuickSync adapter boundary defined. |

### 2.2 Client Hardware Decoding (Media Foundation & NVDEC)

| Decoder Implementation | GPU Vendor | Format | Status | Latency Profile |
|---|---|---|---|---|
| **Microsoft MFT Hardware NVDEC** | NVIDIA | H.264 (NV12) | **Verified Reference** | 2.0 – 2.5 ms decode; 0 frame loss with drain loop. |
| **Microsoft MFT Hardware AMF** | AMD | H.264 (NV12) | **Supported** | Hardware accelerated via DXVA2 / Direct3D 11. |
| **Microsoft MFT Hardware QSV** | Intel | H.264 (NV12) | **Supported** | Supported on Intel Iris Xe and Arc discrete GPUs. |
| **Microsoft MFT Software Fallback** | Any (CPU) | H.264 (YV12/NV12) | **Supported Fallback** | Automatic fallback if GPU decoder initialization fails. |

---

## 3. Display Resolutions & Refresh Rates

| Resolution | Refresh Rate | Pixel Clock | Profile Recommendation | Tested Status |
|---|---|---|---|---|
| **1280x720 (720p)** | 60 Hz | ~74.25 MHz | `Performance` (3,500 kbps) | **Verified** |
| **1920x1080 (1080p)** | 60 Hz | ~148.5 MHz | `Balanced` (8,000 kbps) / `Quality` (15,000 kbps) | **Verified Primary** |
| **2560x1440 (1440p)** | 60 Hz | ~241.5 MHz | `Quality` (18,000 kbps) | **Compatible** |
| **3840x2160 (4K)** | 60 Hz | ~594.0 MHz | `Quality` (25,000 kbps) | **Compatible** (Requires RTX 2060+) |
| **1080p / 1440p** | 120 / 144 Hz | High | Custom `--fps 120` | **Compatible** |

---

## 4. Audio Subsystem Compatibility

| Endpoint Type | Format / Sample Rate | Channels | Status | Notes |
|---|---|---|---|---|
| **WASAPI Loopback (Shared)** | IEEE Float32 @ 48,000 Hz | 2 (Stereo) | **Verified Reference** | Zero-copy shared mode; 10 ms packet cadence. |
| **WASAPI Loopback (Shared)** | PCM 16-bit / 24-bit @ 44.1 kHz | 2 (Stereo) | **Supported** | Hardware resampled to 48 kHz internally for Opus. |
| **WASAPI Surround (5.1 / 7.1)** | Multi-channel PCM | 6 / 8 Channels | **Supported** | Downmixed to stereo before Opus encoder input. |
| **Exclusive Mode Endpoints** | Any | Any | **Unsupported** | Shared mode required for Windows audio loopback tapping. |

---

## 5. Input Subsystem & Peripheral Compatibility

| Input Category | Protocol / API | Injection Method | Status | Notes |
|---|---|---|---|---|
| **Standard Keyboard** | Win32 / RawInput | `SendInput` (Scan Codes) | **Verified** | Anti-cheat safe scan code mapping; host override active. |
| **High-DPI Gaming Mouse** | Win32 / RawInput | `SendInput` (Relative MOUSEEVENTF) | **Verified** | Sub-millisecond pointer tracking; cursor lock supported. |
| **Physical Gamepads** | XInput (0 to 3) | Polling via `XInputGetState` | **Verified** | Triggers, sticks, buttons, D-pad captured on client. |
| **Virtual Gamepad Injection** | ViGEmBus Kernel Driver | `vigem_target_x360_alloc` | **Supported** | Requires ViGEmBus driver on host. |
| **Virtual Gamepad Fallback** | User-Mode Simulation | Memory-mapped state tracker | **Verified Fallback** | Used automatically if ViGEmBus driver is not installed. |

---

## 6. Network & Firewall Traversal Matrix

| Network Topology | Media Path | Candidate Type | Status | Required Ports |
|---|---|---|---|---|
| **Direct Local LAN** | Peer-to-Peer | `host` | **Verified Reference** | Ephemeral UDP |
| **Direct Public WAN (UPnP/PMP)** | Peer-to-Peer | `srflx` (STUN) | **Verified** | UDP 3478 (STUN) + Ephemeral UDP |
| **Symmetric / Enterprise NAT** | Relayed Proxy | `relay` (TURN) | **Verified** | UDP/TCP/TLS 3478/5349 (TURN) |
| **Strict Firewall (TCP Only)** | Relayed Proxy | `relay` (TURN-TCP) | **Supported** | TCP 3478 / 443 |
