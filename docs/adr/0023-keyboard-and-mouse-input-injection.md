# ADR 0023: Keyboard and Mouse Input Injection via Windows SendInput

## Status
Accepted

## Context
Injecting remote user keyboard and mouse input on Windows host systems requires high-performance synthetic event synthesis that works reliably across native Win32 desktop apps, fullscreen DirectX/Vulkan games, and multi-monitor display arrays without kernel-mode driver dependencies.

## Decision
1. **Windows `SendInput` Abstraction:**
   - Encapsulate all synthetic input generation behind `IKeyboardInjector` (`WindowsKeyboardInjector`) and `IMouseInjector` (`WindowsMouseInjector`).
   - Use the Win32 `SendInput` API, which injects inputs ahead of other pending hardware events in the system input stream.
2. **Scan-Code Centric Keyboard Injection:**
   - Always inject keyboard events using hardware scan codes (`KEYEVENTF_SCANCODE` with `ki.wVk = 0`).
   - Physical scan codes ensure consistent WASD gaming controls regardless of the host's active software keyboard layout (e.g. QWERTY vs AZERTY).
   - If a client supplies only virtual key codes, translate dynamically using `MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)`.
   - Respect extended keys (`KEYEVENTF_EXTENDEDKEY`) for navigation clusters, right Alt/Ctrl, and Numpad Enter.
3. **Dual-Mode Mouse Injection:**
   - **Relative Motion:** Injects raw integer deltas (`dx`, `dy`) with `MOUSEEVENTF_MOVE`. Essential for FPS and 3D gaming camera rotation where the cursor is hidden/locked.
   - **Absolute Positioning:** Injects normalized floating-point coordinates `[0.0, 1.0]` transformed into `[0, 65535]` range across the virtual desktop bounding box (`SM_XVIRTUALSCREEN`, `SM_YVIRTUALSCREEN`, `SM_CXVIRTUALSCREEN`, `SM_CYVIRTUALSCREEN`) using `MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE`.
4. **State Tracking & Emergency Release:**
   - Track every currently held scan code in `heldScanCodes_` and every held mouse button in `heldButtonsMask_`.
   - On session disconnect, focus loss, or permission revocation, execute `releaseAllKeys()` and `releaseAllButtons()` to dispatch immediate `KEYUP` and button-up events.
5. **Security & UIPI Boundaries:**
   - Respect Windows User Interface Privilege Isolation (UIPI). A standard user-mode host process cannot inject synthetic input into elevated (UAC) administrator windows. Production deployments requiring UAC interaction must run via an elevated Windows Service with `UIAccess=true` and explicit user consent.

## Consequences
- Studio-grade responsive input injection with measured processing latency < 1 ms.
- Zero kernel driver requirements for keyboard and mouse control.
- Deterministic prevention of stuck keys or persistent mouse clicks upon network interruption.
