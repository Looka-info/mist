# ADR 0033: Windows Application Distribution and Installer Architecture

## Status
Accepted

## Context
Deploying Mist to end-user gaming PCs requires a robust, self-contained Windows distribution package that installs the host, client, desktop shell, and signaling microservice cleanly into standard Windows user locations without requiring administrator privileges or bundling unnecessary developer SDKs.

## Decision
1. **Target Packaging Architecture:**
   - Evaluated MSIX, WiX MSI, and standalone script-driven Windows distribution.
   - For user-mode portable distribution, package a standalone distribution directory (`dist/mist-windows-x64/`) and ZIP archive (`dist/mist-v0.1.0-windows-x64.zip`).
   - Bundle production binaries:
     - `mist_host.exe`: Host streaming engine and CLI diagnostics.
     - `mist_client.exe`: Hardware video decoder and presenter.
     - `mist_desktop.exe`: Native desktop management application.
     - `mist-signaling.exe`: Rust WebSocket signaling microservice.
     - `config/mist.ini`: Validated default configuration.
2. **Automated Install & Uninstall Lifecycle:**
   - **`install.ps1`:**
     - Installs cleanly to `%LOCALAPPDATA%\Programs\Mist`.
     - Creates Start Menu shortcut (`%APPDATA%\Microsoft\Windows\Start Menu\Programs\Mist\Mist Desktop.lnk`).
     - Registers Windows Uninstall entry in registry (`HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Mist`).
   - **`uninstall.ps1`:**
     - Deletes program files, Start Menu shortcuts, and registry entries cleanly.
3. **Optional Dependency Isolation:**
   - Virtual gamepad emulation drivers (ViGEmBus) are explicitly decoupled.
   - The installer never performs silent third-party kernel driver installations. If ViGEmBus is absent, Mist continues to run video, audio, and keyboard/mouse streaming with graceful fallback.

## Consequences
- Clean, non-admin installation and uninstallation verified on Windows 11.
- Compact distribution package (~4.4 MB total archive size).
- Zero risk of system driver corruption or installation failures.
