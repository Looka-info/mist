# Mist Testing & Verification Strategy

## 1. Overview

Mist employs a multi-tiered automated testing strategy:
1. **Unit Tests:** Fast, isolated tests for utilities, state machines, configuration validation, security primitives, and platform abstractions.
2. **Integration / Lifecycle Tests:** Multi-threaded logging safety, session observer dispatches, and application lifecycle sequences.
3. **Smoke Tests:** End-to-end executable execution verifying CLI argument parsing, non-interactive self-test mode, and graceful shutdown.
4. **CI Matrix:** Continuous integration pipeline running on Windows runners with strict warning-as-error policies.

---

## 2. Test Suites

### 2.1 Unit Tests (`mist_unit_tests.exe`)

| Test Suite | File | Coverage |
|---|---|---|
| `CommonTest` | `TestCommon.cpp` | `Error` formatting, `SubsystemId` conversion, `Result<T>` value/error paths, `Result<void>` success/error semantics. |
| `LoggerTest` | `TestLogger.cpp` | Minimum log level filtering, custom sink callbacks, sensitive pattern redaction (`[REDACTED_SENSITIVE_DATA]`), multithreaded logging concurrency. |
| `ConfigTest` | `TestConfig.cpp` | Safe default configuration validation, boundary checks on ports, FPS, bitrates, INI serialization and deserialization, invalid schema rejection. |
| `SessionStateTest` | `TestSessionState.cpp` | Valid and invalid `SessionState` transitions, state machine thread-safety, observer dispatch on state changes. |
| `ApplicationLifecycleTest` | `TestApplicationLifecycle.cpp` | `HostApplication` and `ClientApplication` CLI parsing (`--help`, `--version`, `--self-test`), unknown argument rejection, run cycle, graceful shutdown. |
| `SecurityTest` | `TestSecurity.cpp` | `DeviceIdentity` validation, `AuthToken` expiration logic, `SessionCredentials` validation, `constantTimeEquals` side-channel protection. |
| `TelemetryTest` | `TestTelemetry.cpp` | Atomic counters, gauges, bounded latency histogram calculation (min, max, average, p50, p95, p99), metric registry singleton. |
| `PlatformWindowsTest` | `TestPlatformWindows.cpp` | Win32 error code mapping to Mist errors, HRESULT mapping, `HighResolutionTimer` monotonicity. |

### 2.2 Smoke Tests (CTest)

| Test Name | Target | Purpose |
|---|---|---|
| `smoke_host_version` | `mist_host.exe --version` | Verifies host outputs version 0.1.0 and exits cleanly with code 0. |
| `smoke_host_self_test` | `mist_host.exe --self-test` | Runs host self-test suite (config check, timer check, logger verification) non-interactively. |
| `smoke_client_version` | `mist_client.exe --version` | Verifies client outputs version 0.1.0 and exits cleanly with code 0. |
| `smoke_client_self_test` | `mist_client.exe --self-test` | Runs client self-test suite non-interactively. |

---

## 3. Running Tests

### Automated Suite
```powershell
powershell -ExecutionPolicy Bypass -File scripts\verify-milestone-0.ps1
```

### Via CTest Directly
```powershell
ctest --test-dir build/release -C Release --output-on-failure --verbose
```

### Direct GoogleTest Runner
```powershell
./build/release/tests/unit/mist_unit_tests.exe
```
