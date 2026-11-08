# Mist Release & Code Signing Process

## 1. Overview

This document describes the end-to-end production build, code signing, and release verification process for Mist Native Game Streaming.

---

## 2. Prerequisites

- Windows 10/11 x64 build environment.
- Clang++ (LLVM-MinGW UCRT x86_64) and CMake 3.25+.
- Rust toolchain (`cargo`, `rustc`) for building `mist-signaling.exe`.
- NVIDIA Video Codec SDK headers (`nvEncodeAPI.h`).
- Microsoft Authenticode Code Signing Certificate installed in `Cert:\CurrentUser\My` (or automated development fallback).

---

## 3. Release Build Sequence

1. **Clean Release Compilation:**
   ```powershell
   cmake --build build --config RelWithDebInfo
   ```

2. **Automated Packaging and Code Signing:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts\build-installer.ps1
   ```

   The script automatically:
   - Stages `mist_host.exe`, `mist_client.exe`, `mist_desktop.exe`, and `mist-signaling.exe` into `dist\mist-windows-x64\bin\`.
   - Generates production `dist\mist-windows-x64\config\mist.ini`.
   - Generates non-interactive installer (`install.ps1`) and uninstaller (`uninstall.ps1`).
   - Invokes `scripts\sign-release.ps1` to sign binaries and generate SHA-256 hashes.
   - Compresses the staged directory into `dist\mist-v0.1.0-windows-x64.zip`.
   - Executes install and uninstall validation tests.

---

## 4. Release Verification Checklist

- [ ] All unit tests pass cleanly (`mist_unit_tests.exe`: 0 failures).
- [ ] Host diagnostics pass (`--system-info`, `--dependency-audit`, `--encoder-info`).
- [ ] Client diagnostics pass (`--system-info`, `--dependency-audit`, `--decoder-info`).
- [ ] Native Desktop UI self-test passes (`mist_desktop.exe --self-test`).
- [ ] Authenticode signatures verified on all binaries.
- [ ] SHA-256 hashes recorded in `release-manifest.json`.
- [ ] Distribution archive `dist\mist-v0.1.0-windows-x64.zip` generated.
- [ ] Clean install and uninstall lifecycle verified.
