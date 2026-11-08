# Mist Milestone 5 Implementation Plan
## Native Remote Keyboard, Mouse & Gamepad Control

### 1. Objective & Scope
The objective of Milestone 5 is to implement a native, ultra-low-latency, and secure remote input control subsystem for Mist. An explicitly authorized client can control the host PC using local keyboard, mouse, and game controllers while receiving synchronized 1080p60 H.264 video and 48kHz Opus audio over WebRTC.

#### Scope Boundaries
- **In Scope:**
  - Client-side keyboard capture (Win32 scan codes, key-down, key-up, modifiers, extended keys, focus tracking).
  - Client-side mouse capture (relative delta movement, absolute normalized coordinates, buttons, vertical/horizontal wheel).
  - Client-side gamepad capture (XInput polling, Xbox-compatible layout, deadzones, normalized axes, disconnect handling).
  - Compact versioned binary input protocol (sequence tracking, monotonic timestamps, anti-tampering size validation, state snapshots).
  - Low-latency WebRTC transport via SCTP DataChannels (`input-events` reliable-ordered for discrete transitions; `input-state` low-latency for continuous movement/analog sticks).
  - Host-side input validation, rate limiting, and bounded dispatch queues (decoupled from network threads).
  - Host-side input injection via Windows `SendInput` with full extended scancode support and multi-monitor DPI coordinate mapping.
  - Strict host authorization and permission lifecycle (`InputPermissionManager`): input disabled by default, host approval required, local host override, immediate revocation, and comprehensive disconnect cleanup (releasing all held keys/buttons).
  - Virtual gamepad backend evaluation and abstraction: Architecture Decision Record on Windows Virtual HID Framework (VHF) and ViGEmBus; graceful fallback when driver is absent without affecting keyboard/mouse streaming.
  - Telemetry integration in `MetricRegistry` and diagnostic CLI commands (`--input-test`, `--input-info`, `--gamepad-test`, `--allow-remote-input`).
  - Automated unit and integration test suite; PowerShell benchmark verification script (`scripts/verify-milestone-5.ps1` -> `artifacts/milestone-5.json`).
  - Documentation and Architecture Decision Records (ADRs 0022–0026).
- **Explicitly Excluded:**
  - Remote shell access, command execution, arbitrary process spawning.
  - Clipboard sharing or file transfer.
  - Unattended remote administration / backdoor services.
  - UIPI or anti-cheat bypasses.

---

### 2. Architecture & Subsystem Layout

```
[Client Application]
    ├── ClientKeyboardCapture (Win32 Window / Focus Tracker)
    ├── ClientMouseCapture    (Relative / Absolute / Buttons / Wheel)
    └── ClientGamepadCapture  (XInput Polling, Deadzones, Normalization)
               │
               ▼
      [InputProtocol Serialization]
               │
               ▼
    [WebRTC DataChannels]
    ├── "input-events" (Reliable Ordered: Key/Mouse Button Transitions, Permissions)
    └── "input-state"  (Low-Latency Latest: Continuous Movement, Analog Sticks)
               │
═══════════════╪═══════════════════════════════════════════════════════════════
               │
               ▼
[Host Application]
    ├── InputReceiver (Protocol Parsing, Sequence Tracking, Rate Limiting)
    ├── InputPermissionManager (Disabled by Default, Host Approval, Local Override)
    ├── Bounded DispatchQueue (Decoupled from WebRTC Network Thread)
    └── InputDispatcher
         ├── WindowsKeyboardInjector (`SendInput` ScanCodes, Held Key Registry)
         ├── WindowsMouseInjector    (`SendInput` Abs/Rel, Virtual Screen Transform)
         └── VirtualGamepadAdapter   (ViGEm / VHF Abstraction with Driver Fallback)
```

---

### 3. Execution Phases

1. **Phase 1: Input Protocol & Core Types (`engine/input`)**
   - Define compact binary input packet formats in `InputProtocol.h` / `InputProtocol.cpp`.
   - Implement `InputState.h` / `InputState.cpp` with held-state tracking and snapshot generation.
   - Unit tests for serialization, deserialization, size guards, and sequence monotonicity.

2. **Phase 2: Permission & Authorization Lifecycle (`engine/input`)**
   - Implement `InputPermissionManager.h` / `InputPermissionManager.cpp`.
   - Default disabled; explicit host grant required.
   - Local host override detection (suspends remote input when physical host mouse/keys move).
   - Emergency release cleanup on disconnect, revocation, or focus loss.

3. **Phase 3: Host Input Injection (`engine/input`, `platform/windows`)**
   - Implement `WindowsKeyboardInjector` (Windows `SendInput`, `KEYEVENTF_SCANCODE`, `KEYEVENTF_EXTENDEDKEY`, held key tracking).
   - Implement `WindowsMouseInjector` (Windows `SendInput`, relative deltas, virtual screen normalization `[0.0, 1.0] -> [0, 65535]`, buttons, wheels).
   - Implement `InputDispatcher` managing bounded queue dispatch and emergency state release.

4. **Phase 4: Client Input Capture (`engine/input`)**
   - Implement `ClientKeyboardCapture` (window focus tracking, scan code extraction, release-on-focus-loss).
   - Implement `ClientMouseCapture` (relative deltas for gaming, normalized absolute, button state).
   - Implement `ClientGamepadCapture` (XInput dynamic loader, deadzones, normalized axes, disconnect detection).

5. **Phase 5: WebRTC DataChannel Integration (`engine/transport`)**
   - Add `input-events` and `input-state` DataChannels in `DataChannelManager`.
   - Route outgoing client input events and state updates through transport.
   - Wire incoming host DataChannel messages into `InputReceiver` -> `InputPermissionManager` -> `InputDispatcher`.

6. **Phase 6: Virtual Gamepad Evaluation & Abstraction**
   - Write ADR 0026 evaluating Windows Virtual HID Framework vs ViGEmBus.
   - Implement `IVirtualGamepad` interface with pluggable backend and graceful fallback if driver is absent.

7. **Phase 7: CLI Diagnostics & Application Integration**
   - Extend `HostApplication` and `ClientApplication` with `--allow-remote-input`, `--input-test`, `--gamepad-test`, `--input-info`.
   - Add input telemetry counters and processing latency histograms in `MetricRegistry`.

8. **Phase 8: Automated Verification & Documentation**
   - Write unit tests in `tests/unit/TestInputProtocol.cpp`, `TestInputDispatcher.cpp`, `TestInputPermission.cpp`, `TestGamepadCapture.cpp`.
   - Write PowerShell benchmark verification script `scripts/verify-milestone-5.ps1`.
   - Write ADRs 0022–0026, `docs/input-protocol.md`, `docs/milestone-5-report.md`.
   - Update `architecture.md`, `networking.md`, `protocol.md`, `security.md`, `testing.md`.
