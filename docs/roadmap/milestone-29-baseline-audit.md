# Mist — Milestone 29 Baseline & Codebase Audit
## Windows-Only Release Qualification Baseline & MSVC CI Failure Analysis

**Date:** November 9, 2026  
**Repository:** `https://github.com/Looka-info/mist`  
**Starting Commit:** `c83c207f70df4543c284acc98755fcc199612e76`  
**Target Branch:** `feature/milestone-29-windows-release`  
**Prior Preserved Tag:** `v0.3.0-rc2` (Commit: `9adbf6e0c0c9e0d0b7ed179ad634da63441f8533`)  
**Target Release Candidate:** `v0.3.0-rc3`  
**Primary Platform Target:** Windows 11 x64  

---

## 1. Executive Summary

Milestone 29 pivots Mist strictly to a **Windows-only release qualification** cadence. Cross-platform expansion (macOS, Linux, Android) is formally deferred and removed from the release-blocking pipeline. Mist's core value proposition is high-performance, low-latency Windows game-streaming comparable to Parsec.

Following the push of Milestone 28 (`c83c207`), GitHub Actions Workflow Run `#38` (Run ID `42624358818`, Check Run ID `129184179773`) reported a failure on the `windows-x64` runner during Step 7: **`Build C++ Media Engine`**.

This audit reproduces and diagnoses the exact root causes of the Windows MSVC build failure, analyzes multi-config generator discrepancies between local single-config environments (Ninja/GCC/Clang) and CI multi-config environments (Visual Studio 2022 MSVC), and outlines the remediation plan.

---

## 2. GitHub Actions CI Failure Analysis (Run #38)

- **Workflow Run ID:** `42624358818`
- **Job ID:** `129184179773` (`Windows x64 (Native Host & Slint Desktop)`)
- **Runner OS:** `windows-2022`
- **Compiler:** Microsoft Visual C++ Compiler (MSVC `cl.exe` 19.x / Visual Studio 17 2022)

### Step-by-Step Execution Breakdown:
| Step # | Step Name | Result | Diagnostic Summary |
|:---:|:---|:---:|:---|
| 1 | Set up job | **Success** | Runner provisioned. |
| 2 | Check out repository | **Success** | Checked out `c83c207`. |
| 3 | Set up Rust toolchain | **Success** | Rust stable (`x86_64-pc-windows-msvc`). |
| 4 | Set up MSVC Developer Command Prompt | **Success** | `ilammy/msvc-dev-cmd@v1` activated environment. |
| 5 | Run Rust Workspace Tests | **Success** | All 32 cargo tests passed. |
| 6 | Configure CMake Media Engine (Release) | **Success** | CMake configured with `-G "Visual Studio 17 2022" -A x64`. |
| 7 | **Build C++ Media Engine** | **FAILED** | **Exit code 1** during `cmake --build build --config Release --parallel`. |
| 8 | Run CTest Regression Suite | *Skipped* | Not reached due to step 7 failure. |
| 9 | Run Host & Client Self-Tests | *Skipped* | Not reached. |
| 10 | Stage Distribution Archive | *Skipped* | Not reached. |
| 11 | Upload Windows x64 Artifacts | *Skipped* | Not reached. |

---

## 3. Root Cause Analysis

### Finding 1: Multi-Configuration Path Discrepancy in `installer.rc`
- **Component:** `apps/installer/resources/installer.rc.in`, `apps/installer/CMakeLists.txt`
- **Classification:** **Confirmed Defect (Release Blocker)**
- **Root Cause:**
  In `apps/installer/resources/installer.rc.in`, resource payloads were configured via `configure_file` at CMake configure time:
  ```rc
  201 RCDATA "@CMAKE_BINARY_DIR@/apps/desktop/mist_desktop.exe"
  202 RCDATA "@CMAKE_BINARY_DIR@/apps/host/mist_host.exe"
  203 RCDATA "@CMAKE_BINARY_DIR@/apps/client/mist_client.exe"
  204 RCDATA "@CMAKE_SOURCE_DIR@/bin/mist-signaling.exe"
  ```
  In single-config generators (such as Ninja), binaries are emitted directly into `${CMAKE_BINARY_DIR}/apps/host/mist_host.exe`.
  However, in multi-config generators (such as Visual Studio 2022 `-G "Visual Studio 17 2022"`), CMake emits binaries into per-configuration directories:
  `${CMAKE_BINARY_DIR}/apps/host/Release/mist_host.exe`
  `${CMAKE_BINARY_DIR}/apps/client/Release/mist_client.exe`
  When MSVC invokes `rc.exe` to compile `installer.rc`, `rc.exe` attempts to open `@CMAKE_BINARY_DIR@/apps/host/mist_host.exe`, which does not exist in multi-config builds.
  `rc.exe` fails fatally with:
  `fatal error RC2175: resource file not found: .../build/apps/host/mist_host.exe`

### Finding 2: Hardcoded `OBJECT_DEPENDS` in Installer Target
- **Component:** `apps/installer/CMakeLists.txt`
- **Classification:** **Confirmed Defect**
- **Root Cause:**
  Lines 26-33 set:
  ```cmake
  set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/installer.rc
      PROPERTY OBJECT_DEPENDS
          ${CMAKE_BINARY_DIR}/apps/desktop/mist_desktop.exe
          ${CMAKE_BINARY_DIR}/apps/host/mist_host.exe
          ${CMAKE_BINARY_DIR}/apps/client/mist_client.exe
          ${CMAKE_SOURCE_DIR}/bin/mist-signaling.exe
          ${CMAKE_SOURCE_DIR}/resources/mist.ico
  )
  ```
  In Visual Studio 2022, MSBuild evaluates these explicit file dependencies before executing the build. Because `${CMAKE_BINARY_DIR}/apps/host/mist_host.exe` does not exist in multi-config trees, dependency resolution fails.

### Finding 3: Missing `_CRT_SECURE_NO_WARNINGS` and DirectX SDK Header Warnings Under MSVC `/WX`
- **Component:** `CMakeLists.txt` (`mist_warnings` interface)
- **Classification:** **Confirmed Defect**
- **Root Cause:**
  The top-level `CMakeLists.txt` sets `/WX` (treat warnings as errors) when `MIST_TREAT_WARNINGS_AS_ERRORS=ON`.
  However, `mist_warnings` did not define:
  1. `_CRT_SECURE_NO_WARNINGS`: In MSVC, `std::strncpy` (used in `GuestClientSession.cpp:325`, `ClientInputCapture.cpp`, and `HostApplication.cpp:1391`) triggers `C4996` ('strncpy' unsafe). Under `/WX`, `C4996` is treated as a fatal error.
  2. `NOMINMAX`: Without `NOMINMAX`, `<windows.h>` defines `min` and `max` macros that collide with `std::min` and `std::max` in standard C++20 headers.
  3. `/wd4201`: Microsoft DirectX headers (`d3d11.h`, `dxgi.h`, `mftransform.h`) utilize nonstandard nameless structs/unions, triggering `warning C4201`. Under `/WX`, this is fatal.

### Finding 4: Multi-Config Cargo Staging in `apps/desktop/CMakeLists.txt`
- **Component:** `apps/desktop/CMakeLists.txt`
- **Classification:** **Confirmed Defect**
- **Root Cause:**
  In multi-config generators, `CMAKE_BUILD_TYPE` is empty at configure time. Lines 64-67 checked `if(CMAKE_BUILD_TYPE STREQUAL "Release")`, which evaluated to false, leaving `RUST_CONFIG_FLAG` empty and `RUST_TARGET_SUBDIR` as `"debug"`.
  Furthermore, line 70 hardcoded a local developer path `if(EXISTS "D:/rust/target")`. In CI runners, cargo outputs to `target/release/mist-desktop.exe` or `target/x86_64-pc-windows-msvc/release/mist-desktop.exe`.

### Finding 5: Cross-Platform Matrix Clutter Obscuring Windows Readiness
- **Component:** `.github/workflows/ci.yml`
- **Classification:** **Operational Defect**
- **Root Cause:**
  GitHub Actions CI currently groups macOS, Linux, and Android into the primary workflow matrix. Unrelated failures in Linux headless VA-API or Android NDK obscure the status of the Windows release candidate.

---

## 4. Remediation Plan

1. **Phase 1: Windows-Only Release Workflow**
   - Streamline `.github/workflows/ci.yml` to focus on the Windows x64 release pipeline with strict quality gates:
     - Rust format check (`cargo fmt --check`).
     - Rust clippy with warnings-as-errors (`cargo clippy --workspace --all-targets -- -D warnings`).
     - Rust workspace tests (`cargo test --workspace`).
     - CMake Visual Studio 2022 x64 configuration with warnings-as-errors (`/WX`).
     - Complete native C++ build.
     - CTest regression test suite.
     - Standalone host and client self-tests.
     - Installer packaging via `scripts/build-installer.ps1`.
     - Artifact upload.
   - Separate macOS, Linux, and Android into non-blocking experimental jobs.

2. **Phase 2: Fix MSVC Multi-Config Paths & Compiler Options**
   - Update `CMakeLists.txt` to inject `_CRT_SECURE_NO_WARNINGS`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `/wd4100`, and `/wd4201` into `mist_warnings`.
   - Update `apps/installer/CMakeLists.txt` and `resources/installer.rc.in` to use `file(GENERATE ...)` with generator expressions `$<TARGET_FILE:mist_host>` and `$<TARGET_FILE:mist_client>`, guaranteeing that `rc.exe` references the exact multi-config binary paths.
   - Synchronize `apps/desktop/CMakeLists.txt` to stage `mist_desktop.exe` to `$<TARGET_FILE_DIR:mist_host>/mist_desktop.exe` across all build configurations.

3. **Phase 3: Native Media Regression Verification**
   - Run the complete 235-test CTest regression suite.
   - Verify zero regressions and document physical headless skips.

4. **Phase 4: Real Streaming Integration**
   - Execute and document host/client loopback streaming on Windows 11 with live measured metrics (bitrate, fps, rtt, packet loss).

5. **Phase 5: Installer Validation**
   - Verify `scripts/build-installer.ps1` produces `dist/mist-v0.3.0-rc3-windows-x64.zip` and `dist/MistSetup-v0.3.0-rc3.exe` with silent installation, registry keys, and uninstaller support.
