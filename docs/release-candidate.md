# Mist Release Candidate Qualification — v0.2.0-rc1

**Release Tag:** `v0.2.0-rc1`
**Build Configuration:** Release / RelWithDebInfo
**Target Architecture:** Windows 10/11 x86_64
**Date:** November 8, 2026
**Status:** Qualified Release Candidate (Developer Build)

---

## 1. Overview

**Mist v0.2.0-rc1** is the first standalone release candidate of the Mist Native Game Streaming Platform. It packages:
- Direct3D 11 desktop capture (DXGI Desktop Duplication).
- NVIDIA NVENC hardware-accelerated H.264 video encoding with in-flight dynamic bitrate adaptation.
- Native WebRTC media transport (RFC 6184 H.264 RTP, RFC 7587 Opus RTP, SCTP DataChannels).
- WASAPI high-resolution loopback audio capture and low-latency playback with Opus compression.
- Sub-millisecond remote keyboard, mouse, and game controller dispatching.
- Network QoS congestion control (AIMD) and candidate-pair connection type detection.
- Cryptographic device pairing with 30-bit Base32 codes, single-use consumption, and atomic persistence.
- Native C++20 Windows desktop management shell (`mist_desktop.exe`).
- High-performance WebSocket signaling microservice (`mist-signaling.exe`).

---

## 2. Release Package & Cryptographic Manifest

### Distribution Archive
- **Package Archive:** `dist/mist-v0.2.0-rc1-windows-x64.zip`
- **Archive Size:** 4,653,802 bytes (4.44 MB)
- **Archive SHA-256:** `1FC45EC9DA5F0CACC7F6B473326A71BF7C2D3F0B352BDE41434889F05FD9C915`

### Packaged Binaries Manifest (`dist/mist-windows-x64/release-manifest.json`)

| Binary Executable | Size (Bytes) | SHA-256 Checksum | Signature Status | Cert Thumbprint |
|---|---|---|---|---|
| `mist_host.exe` | 5,139,384 | `3E29D2B35D9DB61DC15DA1714F5ED3CC0562B5A6EC34364CF59FFD595E6D785A` | Signed (Dev Cert) | `5BBF1BACD3CDE907ECC41E4001F35F459CEBB86F` |
| `mist_client.exe` | 4,843,960 | `6DF3299169FC3158F78050A59EF4EA9244332B89D28B38B855BDBA052DF64AE5` | Signed (Dev Cert) | `5BBF1BACD3CDE907ECC41E4001F35F459CEBB86F` |
| `mist_desktop.exe` | 217,016 | `1D5999FE2AFDEBDED99DC0C4EE17608278F8A7DB81DECCA6248C6CA62978AF22` | Signed (Dev Cert) | `5BBF1BACD3CDE907ECC41E4001F35F459CEBB86F` |
| `mist-signaling.exe` | 1,920,952 | `4281F99422952433BAAA7A09E4F69151C325C77F143A17C03E4F6A5FBF5AE711` | Signed (Dev Cert) | `5BBF1BACD3CDE907ECC41E4001F35F459CEBB86F` |

### Package Directory Structure
```
dist/mist-windows-x64/
|-- bin/
|   |-- mist_host.exe
|   |-- mist_client.exe
|   |-- mist_desktop.exe
|   +-- mist-signaling.exe
|-- config/
|   +-- mist.ini
|-- install.ps1
|-- uninstall.ps1
+-- release-manifest.json
```

---

## 3. Code Signing & Trust Qualification

> [!WARNING]
> **DEVELOPMENT CODE SIGNING STATUS**
> Binaries in `v0.2.0-rc1` are cryptographically signed using an isolated local development Authenticode certificate (`CN=Mist Development Release Signing`, Thumbprint: `5BBF1BACD3CDE907ECC41E4001F35F459CEBB86F`).
>
> - **Integrity:** Verifies binary integrity against tampering and establishes the SHA-256 release manifest.
> - **Public Trust:** This build is **NOT signed by a public commercial Certificate Authority** (e.g. DigiCert, Sectigo).
> - **SmartScreen:** Running these binaries on a fresh Windows PC will trigger a standard Microsoft Defender SmartScreen "Unknown Publisher" prompt unless the development certificate is imported into `Cert:\LocalMachine\Root` or the file is unblocked. Public SmartScreen reputation requires an Extended Validation (EV) code-signing certificate.

---

## 4. Installation, Upgrade & Removal

### Option A: Standard PowerShell Installation
1. Extract `dist/mist-v0.2.0-rc1-windows-x64.zip` to a temporary directory.
2. In PowerShell, execute:
   ```powershell
   powershell.exe -ExecutionPolicy Bypass -File .\install.ps1
   ```
3. Target installation path: `%LOCALAPPDATA%\Programs\Mist\`.
4. Creates Start Menu shortcut: `Mist Desktop.lnk`.
5. Registers registry key: `HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Mist`.

### Option B: Silent Corporate / Lab Deployment
```powershell
powershell.exe -ExecutionPolicy Bypass -File .\install.ps1 -InstallPath "C:\Tools\Mist" -Silent
```

### Option C: Upgrade from v0.1.0
1. Close any running instances of `mist_desktop.exe`, `mist_host.exe`, `mist_client.exe`, or `mist-signaling.exe`.
2. Extract the new `mist-v0.2.0-rc1-windows-x64.zip`.
3. Run `install.ps1` with the same `-InstallPath`. It overwrites existing binaries and updates `DisplayVersion` to `0.2.0-rc1` while preserving user pairing data in `trusted_devices.json`.

### Option D: Uninstallation
Open Windows **Settings > Apps > Installed apps**, locate **Mist Native Game Streaming Platform**, and click **Uninstall**.
Alternatively, run via PowerShell:
```powershell
powershell.exe -ExecutionPolicy Bypass -File "%LOCALAPPDATA%\Programs\Mist\uninstall.ps1"
```

---

## 5. Dependency Requirements

1. **Host GPU & Driver:**
   - NVIDIA GeForce GTX 10-series or newer (Pascal, Turing, Ampere, Ada Lovelace).
   - NVIDIA Windows Display Driver $\ge 522.25$ (Verified on Driver `617.14`).
   - `nvEncodeAPI64.dll` present in `%WINDIR%\System32` (installed automatically with NVIDIA driver).
2. **Display Session:**
   - Active Windows console session attached to a physical monitor or virtual display adapter.
3. **C Runtime:**
   - Universal C Runtime (`ucrtbase.dll`) — Standard on all Windows 10/11 installations.
4. **Media Foundation:**
   - Windows Media Foundation components (`mfplat.dll`, `mfreadwrite.dll`). On Windows 10/11 "N" editions, install the optional Windows Media Feature Pack.
5. **Optional Kernel Controller Injection:**
   - ViGEmBus kernel driver (`ViGEmBus.sys`). If absent, Mist automatically uses its verified user-mode memory-mapped simulation fallback.

---

## 6. Known Release Candidate Limitations

1. **Public CA Trust:** Lacks public CA signature; requires user acknowledgment on initial launch.
2. **Endurance Benchmarking:** Bounded to 10.0-second sustained tri-stream benchmark runs. Long-duration (30–60 min) multi-hour streaming stability has not been continuously monitored.
3. **TURN Public Field Relay:** TURN CLI flags and candidate gathering logic are implemented and unit-tested, but live streaming over a commercial public TURN server across multi-region WAN was not qualified.
4. **AMD / Intel Host Encoding:** Host GPU capture and encoding is currently restricted to NVIDIA NVENC. AMD AMF and Intel QuickSync host backends are planned for Milestone 8 / v0.3.
