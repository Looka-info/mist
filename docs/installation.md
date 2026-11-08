# Mist Installation and User Guide

## 1. System Requirements

### Host PC (Streaming Source)
- **OS:** Windows 10 or Windows 11 64-bit (Build 19041 or newer).
- **GPU:** NVIDIA GeForce GTX 10-series or newer (Pascal, Turing, Ampere, Ada Lovelace) with NVENC support.
- **Display Driver:** NVIDIA Display Driver 522.25 or newer (recommended: 560+).
- **RAM:** 8 GB minimum (16 GB recommended).
- **Network:** 1 Gbps Ethernet or 5 GHz 802.11ac/ax Wi-Fi.

### Client PC (Viewer & Controls)
- **OS:** Windows 10 or Windows 11 64-bit.
- **GPU:** DirectX 11 Feature Level 11_1 compatible GPU (Intel, AMD, NVIDIA).
- **Audio:** Standard Windows sound card, headphones, or speakers.
- **Input:** Keyboard, Mouse, and/or XInput-compatible game controller.

---

## 2. Installation

1. **Download Release Package:**
   Download `mist-v0.2.0-rc1-windows-x64.zip` from the official release distribution.

2. **Extract Archive:**
   Extract the archive to any temporary folder.

3. **Run Automated Installer:**
   Open PowerShell and execute:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\install.ps1
   ```
   - Installs binaries to `%LOCALAPPDATA%\Programs\Mist\`.
   - Creates a Start Menu shortcut: **Mist Desktop**.
   - Registers standard Windows uninstall entry.

---

## 3. Quick Start Guide

### Launching Mist Desktop
Launch **Mist Desktop** from the Start Menu, or execute:
```powershell
& "$env:LOCALAPPDATA\Programs\Mist\mist_desktop.exe"
```

### Device Pairing Workflow
1. On the **Host PC**, navigate to the **PAIRING** screen.
2. Note the generated 6-character pairing code (e.g. `5F1-2U8`).
3. On the **Client PC**, enter the pairing code to request connection.
4. On the **Host PC**, approve the pairing request. Permissions for video, audio, and inputs can be toggled individually.

### Uninstallation
To cleanly uninstall Mist, open Windows **Installed Apps** and select **Uninstall**, or execute:
```powershell
powershell -ExecutionPolicy Bypass -File "$env:LOCALAPPDATA\Programs\Mist\uninstall.ps1"
```
