# Mist Release Candidate Qualification — v0.2.0-rc1

**Release Tag:** `v0.2.0-rc1`  
**Target Architecture:** Windows 10/11 x86_64  
**Date:** November 8, 2026  
**Status:** Qualified Release Candidate  

---

## 1. Overview

**Mist v0.2.0-rc1** is the first production release candidate of the Mist Native Game Streaming Platform. It brings together native Direct3D 11 GPU desktop capture, hardware-accelerated NVIDIA NVENC H.264 encoding, native WebRTC peer-to-peer transport, low-latency WASAPI loopback audio with Opus compression, sub-millisecond remote keyboard/mouse/gamepad control, adaptive network QoS congestion control, and cryptographic device pairing into a standalone Windows distribution package.

---

## 2. Release Package & Checksums

### Distribution Archive
- **Filename:** `dist/mist-v0.2.0-rc1-windows-x64.zip`
- **Archive Size:** 4.44 MB (4,660,192 bytes)
- **Archive SHA-256:** Run `Get-FileHash dist/mist-v0.2.0-rc1-windows-x64.zip -Algorithm SHA256`

### Packaged Binaries Manifest (`dist/mist-windows-x64/release-manifest.json`)

| Binary | Size (Bytes) | SHA-256 Hash | Code Signing Status |
|---|---|---|---|
| `mist_host.exe` | 5,139,384 | `3E29D2B35D9DB61DC15DA1714F5ED3CC0562B5A6EC34364CF59FFD595E6D785A` | Authenticode Signed |
| `mist_client.exe` | 4,843,960 | `6DF3299169FC3158F78050A59EF4EA9244332B89D28B38B855BDBA052DF64AE5` | Authenticode Signed |
| `mist_desktop.exe` | 217,016 | `1D5999FE2AFDEBDED99DC0C4EE17608278F8A7DB81DECCA6248C6CA62978AF22` | Authenticode Signed |
| `mist-signaling.exe` | 1,920,952 | `4281F99422952433BAAA7A09E4F69151C325C77F143A17C03E4F6A5FBF5AE711` | Authenticode Signed |

---

## 3. Quick Start & Installation

### Option A: Automated PowerShell Installer (Recommended)
1. Extract `mist-v0.2.0-rc1-windows-x64.zip` to a temporary directory.
2. Open PowerShell and run:
   ```powershell
   powershell.exe -ExecutionPolicy Bypass -File .\install.ps1
   ```
3. This installs Mist into `%LOCALAPPDATA%\Programs\Mist`, creates a Start Menu shortcut for **Mist Desktop**, and registers an entry in Windows **Apps & Features**.

### Option B: Silent Deployment
```powershell
powershell.exe -ExecutionPolicy Bypass -File .\install.ps1 -InstallPath "C:\Tools\Mist" -Silent
```

### Option C: Standalone Portable Mode
No installation required. You can run `bin\mist_desktop.exe`, `bin\mist_host.exe`, or `bin\mist_client.exe` directly from the extracted directory.

---

## 4. Uninstallation

To cleanly remove Mist:
1. Open Windows **Settings > Apps > Installed apps**, find **Mist Native Game Streaming Platform**, and click **Uninstall**.
2. Or run directly from PowerShell:
   ```powershell
   powershell.exe -ExecutionPolicy Bypass -File "%LOCALAPPDATA%\Programs\Mist\uninstall.ps1"
   ```

---

## 5. Verification & Testbed Results

| Subsystem | Metric | Verified Result | Target Specification | Status |
|---|---|---|---|---|
| **Video Encode** | Actual FPS | 59.90 FPS | 60.00 FPS | **PASSED** |
| **Video Bitrate** | Average Throughput | 11.53 Mbps | 8.0 – 15.0 Mbps | **PASSED** |
| **Video Decode** | Decode Efficiency | 100.0% (563/563) | 100.0% | **PASSED** |
| **Video Queue** | Dropped / Discarded | 0 frames | 0 frames | **PASSED** |
| **Video Decode Errors** | Hardware MFT Errors | 0 errors | 0 errors | **PASSED** |
| **Audio Playout** | Samples Rendered | 719,040 PCM | Continuous | **PASSED** |
| **A/V Sync Skew** | 95th Percentile Skew | 17.50 ms | $< 40$ ms | **PASSED** |
| **Input Execution** | Remote Events Injected | 179 events | Zero drops | **PASSED** |
| **Automated Tests** | GoogleTest Suite | 112 passed, 2 skipped | 100% of runnable | **PASSED** |

---

## 6. Known Limitations & Release Constraints

1. **Authenticode Public CA Certificate:**
   - Binaries in this RC are signed with a development Authenticode certificate for cryptographic integrity and manifest generation.
   - For general untrusted end-user distribution outside development environments, an EV Code Signing certificate from a trusted public root CA (e.g. DigiCert, Sectigo) must be applied to prevent Microsoft Defender SmartScreen prompts.
2. **Display Session Requirements:**
   - DXGI Desktop Duplication requires an active Windows graphical session attached to a physical or virtual display. Running in headless RDP sessions without GPU acceleration is not supported.
3. **Virtual Gamepad Emulation:**
   - Full virtual Xbox 360 controller injection requires the third-party **ViGEmBus** kernel driver. When the driver is not present, Mist automatically falls back to an internal user-mode simulation layer without failing.
4. **Hardware Encoder Vendor:**
   - Host GPU capture and hardware encoding currently target NVIDIA NVENC (Pascal, Turing, Ampere, Ada Lovelace). AMD AMF and Intel QuickSync host encoders are architecturally isolated behind adapter interfaces and scheduled for Milestone 8 / v0.3.
