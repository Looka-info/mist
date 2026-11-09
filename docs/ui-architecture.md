# Mist Desktop UI Architecture

## 1. Architectural Overview

Mist Desktop is an ultra-low latency, native Windows management application designed specifically for game-streaming workflows. Unlike traditional streaming clients that bundle heavyweight web runtimes (such as Electron or Chromium Embedded Framework), Mist Desktop is built with **zero web dependencies**:

```
+---------------------------------------------------------------------------------+
|                                 Mist Desktop UI                                 |
+---------------------------------------------------------------------------------+
|                                                                                 |
|   +------------------------------------+   +--------------------------------+   |
|   |         Navigation & Screen        |   |       Mint Design Tokens       |   |
|   |   Home | Pairing | Host | Settings |   |  Light / Dark Mode State Engine|   |
|   |   Live Session | Device Management |   |  Material 3 Shapes & Spacing   |   |
|   +------------------------------------+   +--------------------------------+   |
|                     |                                       |                   |
|                     v                                       v                   |
|   +-------------------------------------------------------------------------+   |
|   |                          Dear ImGui v1.91.8                             |   |
|   +-------------------------------------------------------------------------+   |
|                     |                                       |                   |
|                     v                                       v                   |
|   +------------------------------------+   +--------------------------------+   |
|   |    imgui_impl_win32 (Win32 API)    |   |  imgui_impl_dx11 (Direct3D 11) |   |
|   |    High-DPI Per-Monitor v2 Aware   |   |  Hardware / WARP Presentation  |   |
|   +------------------------------------+   +--------------------------------+   |
|                     |                                       |                   |
|                     v                                       v                   |
|   +-------------------------------------------------------------------------+   |
|   |            Windows 10 / 11 Kernel, User32 & D3D11 Runtime               |   |
|   +-------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------+
```

---

## 2. Windowing & Hardware Presentation Pipeline

### 2.1 Win32 Subsystem & High-DPI Awareness
- **Initialization:** Handled in `initializeD3D11Gui()` in `apps/desktop/src/DesktopApplication.cpp`.
- **DPI Scaling:** Configured via `platform::windows::initializeProcessDpiAwareness()`, enabling `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` on Windows 10/11. The layout dynamically scales fonts, padding, and shapes without blurring.
- **Message Dispatch:** Custom `DesktopWndProc` routes messages to `ImGui_ImplWin32_WndProcHandler`. Window resizing triggers `handleResize(width, height)`, cleanly re-allocating `IDXGISwapChain1` buffers and `ID3D11RenderTargetView`.

### 2.2 Direct3D 11 Rendering Backend
- **Adapter Strategy:** Attempts hardware device creation via `D3D_DRIVER_TYPE_HARDWARE` targeting feature levels `11_1`, `11_0`, and `10_0`.
- **WARP Software Fallback:** If hardware creation fails (e.g., automated test runners, virtualized CI nodes, or remote sessions), the pipeline falls back automatically to `D3D_DRIVER_TYPE_WARP`, ensuring reliable execution across all Windows environments.
- **Presentation Latency:** Swapchain runs with `DXGI_SWAP_EFFECT_DISCARD` and synchronous `Present(1, 0)` VSync lock, consuming less than 0.4 ms frame time and maintaining zero CPU/GPU overhead while streaming games.

---

## 3. Design System & Theme Engine

### 3.1 Tokenized Theming Engine (`MintDesignTokens.h`)
The application abstracts all colors, shapes, and metrics into semantic tokens. The interface never references hardcoded hex constants in component code.

```cpp
// Runtime theme mode state in DesktopApplication
tokens::ThemeMode currentTheme_{tokens::ThemeMode::Light};

// Toggling theme dynamically updates ImGui styles and clear colors
void DesktopApplication::toggleTheme() {
    currentTheme_ = (currentTheme_ == tokens::ThemeMode::Light)
        ? tokens::ThemeMode::Dark
        : tokens::ThemeMode::Light;
    applyActiveTheme();
}
```

### 3.2 Material 3 Card Engine
All screens use the uniform `BeginM3CardHelper` / `EndM3CardHelper` pattern:
- Enforces `ChildRounding = 24.0f` (Large container radius).
- Applies `cardBg` and `cardBorder` from active semantic tokens.
- Standardizes internal padding to `S16` (16px).

---

## 4. Screen Architecture & Workflows

### 4.1 Screen 1: Home (Dashboard & Quick Actions)
- **Hero Card:** Displays 44px sleek geometric logo, brand tagline, and overview.
- **Action Tiles:** 4 Material 3 24px-rounded action tiles for Wi-Fi Screen Share, Connect with Code, Paired Devices, and Wi-Fi & Network.
- **Latest Sessions:** Quick review of recent connections and streaming metrics.
- **System Check:** Real-time summary of GPU encoder (NVENC), graphics driver, display session, audio endpoint, and Wi-Fi link.

### 4.2 Screen 2: Wi-Fi Screen Share (Host)
- **Wi-Fi Preflight Banner:** Validates active Wi-Fi connection, SSID, signal quality, NVENC encoder, and WASAPI loopback before streaming.
- **Active Code Display:** Prominent 6-character pill boxes with animated pulse highlight, live expiration countdown timer (300 seconds), and single-use invalidation.
- **One-Click Session Controls:** Copy Code to clipboard, Revoke & New Code, and Stop Sharing.
- **Host Permission Controls:** Explicit toggles for streaming game audio and allowing remote input (keyboard, mouse, gamepad).
- **Incoming Requests:** Interactive approval cards displaying requesting device identity and permission requests.

### 4.3 Screen 3: Connect to a Device (Client)
- **6-Character Code Entry:** Single unified pairing entry point; supports typing or pasting a 6-character code from clipboard.
- **Permission Preferences:** Toggles for receiving game audio and sending remote input.
- **Animated Stepper:** Step-by-step progress visualizer tracking network verification, code authentication, WebRTC negotiation, and first-frame delivery.
- **Automatic Session Transition:** On successful pairing, smoothly navigates into the Live Streaming Session.

### 4.4 Screen 4: Stream Quality Settings
- **Quality Profiles:** 4 interactive Material 3 profile options:
  - *Performance:* 720p60, 3,500 kbps (Competitive sub-15ms esports).
  - *Balanced:* 1080p60, 8,000 kbps (High-speed Wi-Fi / direct link default).
  - *Quality:* 1080p60, 15,000 kbps (Pristine visual fidelity).
  - *Adaptive:* Dynamic 2,000–20,000 kbps (AIMD congestion-controlled).
- **Custom Tuning:** Bitrate sliders (Min, Target, Max) and frame rate selector.

### 4.5 Screen 5: Live Streaming Session & Floating HUD
- **Active Viewport:** Large immersive viewport representation displaying the live 60 FPS hardware stream.
- **Collapsible Floating Glass HUD:** Semi-transparent Material Design 3 HUD displaying live telemetry:
  - *Resolution & FPS:* 1080p60 NVENC
  - *Bandwidth & Bitrate:* Real-time throughput (Mbps)
  - *Latency:* Sub-millisecond to low-millisecond RTT (ms), jitter, and packet loss
  - *Status Indicators:* Stereo Opus 48 kHz audio and remote input control status
  - *Controls:* Fullscreen mode toggle, HUD minimize/expand toggle, and graceful stream disconnect action.

### 4.6 Screen 6: Device Management
- **Trusted Device Ledger:** Persisted device list loaded via `PairingManager`.
- **Permission Badges:** Granular pills for Keyboard (`KBD`), Mouse (`MOUSE`), and Gamepad (`PAD`).
- **Cryptographic Revocation:** Instantly invalidates Ed25519 pairing token and severs associated session DataChannels.

---

## 5. Non-Interactive CLI Mode & Automated Testability

To support automated test runners and CI verification without requiring a physical monitor or interactive user input:
- `mist_desktop --self-test`: Executes an automated traversal of all 6 screens, validates profile configurations, prints standard output, and exits with code 0.
- `mist_desktop --home`, `--pairing`, `--host-status`, `--settings`, `--live-session`, `--devices`: Output structured console reports and exit cleanly.
- `mist_desktop --profile <name>`: Configures stream profiles headlessly.
- Unit testing is covered by `TestDesktopApp.cpp` in `mist_unit_tests.exe`.
