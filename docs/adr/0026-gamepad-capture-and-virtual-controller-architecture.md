# ADR 0026: Gamepad Capture and Virtual Controller Architecture

## Status
Accepted

## Context
Remote game streaming requires capturing client-side game controllers (analog sticks, triggers, directional pads, face buttons) and recreating them on the host PC so that host games perceive a native physical gamepad. 

On Windows, physical gamepads are exposed to user applications via XInput or DirectInput/Windows.Gaming.Input. However, injecting synthetic controller inputs on the host requires emulating a physical USB HID or XUSB device at the operating system level.

## Decision
1. **Client-Side Gamepad Capture via Dynamic XInput:**
   - The client captures controller input using XInput (`xinput1_4.dll` dynamically resolved, falling back to `xinput9_1_0.dll`).
   - Dynamic loading ensures the client binary executes cleanly across all Windows versions without hard compile-time DLL binding.
   - Standard hardware deadzones and thresholds are applied before transmission:
     - Left thumbstick deadzone: 7,849 (out of 32,767).
     - Right thumbstick deadzone: 8,689 (out of 32,767).
     - Trigger threshold: 30 (out of 255).
   - Only controllers with changed state or active analog input are transmitted, reducing unnecessary DataChannel traffic.
2. **Evaluation of Windows Virtual HID Framework (VHF) vs ViGEmBus:**
   - **Windows Virtual HID Framework (VHF):** VHF allows user-mode software to write a driver that feeds HID reports. However, VHF drivers require a Kernel-Mode Driver Framework (KMDF) wrapper, Extended Validation (EV) code-signing certificates, and Microsoft Windows Hardware Quality Labs (WHQL) driver attestation. Authoring and distributing a custom kernel driver introduces severe system instability risks (BSOD), signing costs, and maintenance burdens.
   - **ViGEmBus (Virtual Gamepad Emulation Bus):** ViGEmBus is the proven, open-source, WHQL-certified kernel driver widely adopted by game streaming solutions (Parsec, Moonlight, Steam Input). It exposes clean user-mode client APIs for spawning virtual Xbox 360 and DualShock 4 controllers.
3. **Pluggable Abstraction & Graceful Driver Fallback:**
   - Implement `IVirtualGamepad` with `HostVirtualGamepad` encapsulating virtual controller management.
   - Dynamically probe for the ViGEmBus driver at runtime.
   - **Crucial Rule:** Never attempt silent or unattended kernel driver installation.
   - If the ViGEmBus driver is absent, `HostVirtualGamepad` activates a graceful fallback mode:
     - Host video, audio, keyboard, and mouse streaming operate normally without interruption.
     - Diagnostic commands (`--input-info`, `--gamepad-test`) truthfully report `Driver unavailable; graceful fallback active`.
     - Telemetry logs record the absence of the driver without raising fatal error conditions.

## Consequences
- Client supports standard Xbox and compatible gamepads out of the box with zero third-party drivers.
- Host provides virtual controller injection when ViGEmBus is installed.
- Zero crashes, failures, or deployment blockers when host runs on environments without virtual gamepad drivers.
