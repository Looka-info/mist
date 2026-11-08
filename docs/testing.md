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
| `CaptureTest` | `TestCapture.cpp` | Direct3D 11 device creation on adapter 0, DXGI display enumeration, live desktop duplication acquisition/release, synthetic frame generation. |
| `EncoderTest` | `TestEncoder.cpp` | NVENC API runtime loading, hardware capability query, session initialization, H.264 NAL start-code bitstream validation. |
| `PipelineTest` | `TestPipeline.cpp` | Bounded frame queue (max 2), frame dropping on lag, synthetic capture-to-NVENC pipeline benchmark, report serialization. |
| `StabilityTest` | `TestStability.cpp` | Repeated capture initialization/teardown (5 iterations), repeated encoder session initialization/teardown (5 iterations) without memory/VRAM leaks. |
| `H264ParserTest` | `TestH264Parser.cpp` | Annex-B start-code search (3-byte/4-byte), NAL parsing, AUD/SPS/PPS/IDR detection, Exp-Golomb bit reading, access unit extraction. |
| `AnnexBReaderTest` | `TestAnnexBReader.cpp` | Offline stream loading, access-unit partitioning, stream metadata estimation, size guards. |
| `DecoderTest` | `TestDecoder.cpp` | Direct3D 11 hardware decoder capability discovery, live hardware decoding of H.264 bitstream to `ID3D11Texture2D` NV12 surfaces. |
| `RendererTest` | `TestRenderer.cpp` | `D3D11Renderer` headless initialization, synthetic frame presentation, dynamic viewport resize, vsync toggle, clean shutdown. |
| `ClientPipelineTest` | `TestClientPipeline.cpp` | End-to-end paced decode and present pipeline on test bitstream, non-zero FPS, JSON report generation. |
| `ClientStabilityTest` | `TestClientStability.cpp` | Repeated decoder, renderer, and client pipeline creation/teardown (5 iterations each) without GPU resource leaks. |
| `TestWebRtcTransport` | `TestWebRtcTransport.cpp` | WebRTC transport configuration, statistics tracking, dual peer initialization, in-process direct loopback ICE negotiation, H.264 RTP packetization, and RTCP PLI keyframe request. |
| `TestSignaling` | `TestSignaling.cpp` | Signaling message type encoding/decoding, session registration payloads, message serialization. |
| `TestNetworkStability` | `TestNetworkStability.cpp` | Repeated WebRTC transport open/close cycles (3 iterations), frame queue backpressure drop policy (oldest dropped), lock-free statistics accumulation. |

### 2.2 Smoke Tests (CTest)

| Test Name | Target | Purpose |
|---|---|---|
| `smoke_host_version` | `mist_host.exe --version` | Verifies host outputs version 0.1.0 and exits cleanly with code 0. |
| `smoke_host_self_test` | `mist_host.exe --self-test` | Runs host self-test suite (config check, timer check, display check, NVENC hardware probe, test frame encode) non-interactively. |
| `smoke_host_list_displays` | `mist_host.exe --list-displays` | Enumerates DXGI displays and GPU adapters and exits with code 0. |
| `smoke_client_version` | `mist_client.exe --version` | Verifies client outputs version 0.1.0 and exits cleanly with code 0. |
| `smoke_client_self_test` | `mist_client.exe --self-test` | Runs client self-test suite (config check, timer check, D3D11 hardware decoder probe, renderer check) non-interactively. |
| `smoke_client_decoder_info` | `mist_client.exe --decoder-info` | Queries Direct3D 11 hardware decoder capabilities (adapter, H.264 profile, max resolution) and exits with code 0. |
| `smoke_client_present_test_headless` | `mist_client.exe --present-test --headless --fps 60` | Runs headless Direct3D 11 presentation loop (60 frames), verifying timing and zero frame drops. |

---

## 3. Running Tests

### Automated Milestone 3 Verification Suite
```powershell
powershell -ExecutionPolicy Bypass -File scripts\verify-milestone-3.ps1 -BenchmarkSeconds 10
```

### Automated Milestone 2 Verification Suite
```powershell
powershell -ExecutionPolicy Bypass -File scripts\verify-milestone-2.ps1
```

### Milestone 3 WebRTC CLI Diagnostic Commands
```powershell
# Query NVIDIA NVENC runtime driver & SDK 13.1 compatibility audit
./build/apps/host/mist_host.exe --nvenc-audit --report artifacts/nvenc-audit.json

# Query WebRTC transport capabilities
./build/apps/host/mist_host.exe --transport-info
./build/apps/client/mist_client.exe --transport-info

# Run live WebRTC streaming session (1080p60 over signaling)
./bin/mist-signaling.exe --port 8080
./build/apps/client/mist_client.exe --receive-test --signaling-url ws://127.0.0.1:8080 --session-id m3-test --duration 10 --headless --report artifacts/client-stream.json
./build/apps/host/mist_host.exe --stream-test --signaling-url ws://127.0.0.1:8080 --session-id m3-test --duration 10 --report artifacts/host-stream.json
```

### Client CLI Diagnostic Commands
```powershell
# Query Direct3D 11 hardware decoder capabilities
./build/release/apps/client/mist_client.exe --decoder-info

# Run Direct3D 11 presentation loop (headless or windowed)
./build/release/apps/client/mist_client.exe --present-test --headless --fps 60

# Decode test bitstream with hardware acceleration
./build/release/apps/client/mist_client.exe --decode-test --input artifacts/test.h264 --headless --fps 60 --duration 2

# Run full client hardware decode and presentation benchmark
./build/release/apps/client/mist_client.exe --decode-benchmark --input artifacts/test.h264 --report artifacts/milestone-2.json --headless
```

### CLI Diagnostic Commands
```powershell
# Enumerate connected displays and adapters
./build/release/apps/host/mist_host.exe --list-displays

# Run DXGI Desktop Duplication capture test
./build/release/apps/host/mist_host.exe --capture-test --display 0 --duration 2

# Run GPU-resident NVENC hardware encode test
./build/release/apps/host/mist_host.exe --encode-test --codec h264 --width 1280 --height 720 --fps 60 --duration 2 --output artifacts/test.h264

# Run end-to-end capture and hardware encode benchmark
./build/release/apps/host/mist_host.exe --capture-encode-test --display 0 --duration 5 --report artifacts/milestone-1.json
```

### Via CTest Directly
```powershell
ctest --test-dir build/release -C Release --output-on-failure --verbose
```

### Direct GoogleTest Runner
```powershell
./build/release/tests/unit/mist_unit_tests.exe
```
