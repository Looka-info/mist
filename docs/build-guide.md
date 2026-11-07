# Mist Build & Developer Guide

## 1. Prerequisites

### Windows Environment
- **Operating System:** Windows 10 x64 / Windows 11 x64.
- **Build System:** CMake 3.25 or higher.
- **Generator:** Ninja 1.11+ or Visual Studio 17 2022.
- **Compilers Supported:**
  - **MSVC:** Microsoft Visual C++ 2022 (v143 toolset or newer), `/W4`, `/permissive-`, C++20.
  - **Clang:** LLVM Clang++ 17+ (or LLVM-MinGW UCRT targeting `x86_64-w64-windows-gnu`).
- **Package Manager:** vcpkg (manifest mode via `vcpkg.json`).

---

## 2. Automated Verification Script

The recommended way to verify your Windows environment, build Debug/Release configurations, and execute all tests is the automated PowerShell verification script:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\verify-milestone-0.ps1
```

The script will:
1. Detect CMake, Ninja, compilers (MSVC or Clang++), and vcpkg.
2. Configure Debug build with strict warnings.
3. Build all targets.
4. Execute 38 CTest unit and smoke tests.
5. Configure Release build.
6. Build all Release targets.
7. Execute all Release CTest tests.
8. Execute live non-interactive smoke tests for `mist_host` and `mist_client`.

---

## 3. Manual Build Instructions

### 3.1 Using CMake Presets

Configure, build, and test using standard presets defined in `CMakePresets.json`:

#### Debug (MSVC)
```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

#### Release (MSVC)
```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

#### Debug (Clang / Ninja)
```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug
```

#### Release (Clang / Ninja)
```powershell
cmake --preset windows-clang-release
cmake --build --preset windows-clang-release
ctest --preset windows-clang-release
```

---

## 4. Testing

### Run All Unit and Smoke Tests via CTest
```powershell
ctest --test-dir build/release -C Release --output-on-failure --verbose
```

### Run Unit Tests Directly
```powershell
./build/release/tests/unit/mist_unit_tests.exe
```

### Run Executable Smoke Tests
```powershell
# Host self-test
./build/release/apps/host/mist_host.exe --self-test
# Host version
./build/release/apps/host/mist_host.exe --version

# Client self-test
./build/release/apps/client/mist_client.exe --self-test
# Client version
./build/release/apps/client/mist_client.exe --version
```

---

## 5. Troubleshooting

| Issue | Cause | Resolution |
|---|---|---|
| `No compatible C++20 compiler found` | Neither MSVC `cl.exe` nor Clang++ is in PATH. | Launch Developer Command Prompt for VS 2022 or install LLVM-MinGW. |
| `SetProcessDpiAwarenessContext undeclared` | Older SDK headers or MinGW toolchain. | `PlatformWindows.cpp` dynamically retrieves `SetProcessDpiAwarenessContext` from `user32.dll`. Verify you are building the latest master branch. |
| `FetchContent download failure` | Network proxy or offline build. | Provide `vcpkg` with `gtest` pre-installed or ensure internet access for the initial CMake configure step. |
| `Unused result warning error` | Functions tagged `[[nodiscard]]` must have their return values handled or explicitly cast to `(void)`. | Check return value or cast to `(void)` when discarding intentional success/failure signals. |
