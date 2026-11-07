# Milestone 1 Implementation Plan: Native GPU Capture & Hardware Video Encoding

**Project:** Mist Native Game Streaming Platform  
**Milestone:** 1 (Native GPU Capture & Hardware Video Encoding)  
**Lead Engineer:** Lead Senior C++ Graphics, Windows Systems & Video-Encoding Engineer  
**Target Hardware:** AMD Ryzen 5 5600, NVIDIA GeForce GTX 1660 SUPER, Windows 11 x64  

---

## 1. Objectives & Scope

Implement a production-grade, ultra-low-latency Windows GPU capture and H.264 hardware encoding pipeline with zero CPU memory readbacks.

### In Scope
- Direct3D 11 device infrastructure on the appropriate DXGI adapter.
- Display enumeration and metadata tracking.
- DXGI Desktop Duplication (`IDXGIOutputDuplication`) capture engine.
- NVIDIA NVENC hardware encoder integration via dynamic loading of `nvEncodeAPI64.dll`.
- Zero-CPU-readback GPU texture pipeline (BGRA8 and D3D11 Video Processor BGRA-to-NV12 conversion).
- Bounded queue pipeline (maximum 2 pending frames, oldest dropped if encoder lags).
- Synthetic GPU test pattern generator for headless/CI validation.
- Extended `mist_host` CLI diagnostics (`--list-displays`, `--capture-test`, `--encode-test`, `--capture-encode-test`).
- Detailed capture and encoding telemetry (p50, p95, p99 latencies).
- Unit, integration, and stability test suite.
- Architecture Decision Records (ADRs) and Milestone 1 Report.

### Out of Scope (Preserved for Future Milestones)
- WebRTC transport, RTP, STUN/TURN (Milestone 3).
- Remote keyboard, mouse, and gamepad input injection (Milestone 5).
- Audio streaming / WASAPI (Milestone 4).
- Client hardware decoding (Milestone 2).

---

## 2. Architectural Design

```
+---------------------------------------------------------------------------------------------------------+
|                                    MIST GPU CAPTURE & ENCODE PIPELINE                                   |
|                                                                                                         |
|  +---------------------------+     +----------------------------+     +------------------------------+  |
|  |     DXGI Desktop Dupl     |     |     GPU Texture Pool       |     |        NVIDIA NVENC          |  |
|  |   (IDXGIOutputDuplication)|     |  (Bounded Queue: 2 frames) |     |     (nvEncodeAPI64.dll)      |  |
|  +---------------------------+     +----------------------------+     +------------------------------+  |
|                |                                  |                                  |                  |
|       AcquireNextFrame()                          |                                  |                  |
|                v                                  v                                  v                  |
|  [Desktop BGRA8 Surface] ----> [Optional VideoProcessor NV12] ----> [nvEncEncodePicture] -> H.264 NALs  |
|                |                   (Zero CPU Readback)                               |                  |
|                +---------------------------------------------------------------------+                  |
|                                                   |                                                     |
|                                                   v                                                     |
|                                       [Telemetry & Metrics]                                             |
|                                (Acquisition, Encode Latency, FPS)                                       |
+---------------------------------------------------------------------------------------------------------+
```

---

## 3. Subsystem Breakdown & Tasks

### Phase 1: Direct3D 11 Infrastructure & Display Enumeration
- File: `platform/windows/include/mist/platform/windows/D3D11Device.h` & `src/D3D11Device.cpp`
- Enumerate DXGI adapters and outputs (`IDXGIFactory1`, `IDXGIAdapter1`, `IDXGIOutput1`).
- Extract display dimensions, refresh rate, rotation, coordinates, and primary monitor flag.
- Create D3D11 device and immediate context on the specified adapter.
- Query hardware capabilities.

### Phase 2: DXGI Desktop Duplication (`DxgiCaptureEngine`)
- File: `engine/capture/include/mist/capture/DxgiCaptureEngine.h` & `src/DxgiCaptureEngine.cpp`
- Implement `ICaptureEngine`.
- Manage `IDXGIOutputDuplication` lifecycle.
- Handle `DXGI_ERROR_WAIT_TIMEOUT` gracefully.
- Handle `DXGI_ERROR_ACCESS_LOST` by re-creating the duplication interface.
- Extract desktop frame texture and track frame timestamps using `HighResolutionTimer`.
- Bounded texture acquisition using RAII.

### Phase 3: NVIDIA NVENC Hardware Video Encoder (`NvencVideoEncoder`)
- File: `engine/encode/include/mist/encode/NvencVideoEncoder.h` & `src/NvencVideoEncoder.cpp`
- Support CMake discovery via `MIST_NV_CODEC_SDK_ROOT` with graceful fallback to `ErrorCode::HardwareUnavailable` when SDK or GPU is absent.
- Dynamically load `nvEncodeAPI64.dll` via `LoadLibraryA` / `GetProcAddress`.
- Initialize `NV_ENCODE_API_FUNCTION_LIST`.
- Check device capabilities (`nvEncGetEncodeCaps`).
- Open encode session on D3D11 device (`nvEncOpenEncodeSessionEx`).
- Configure low-latency CBR rate control and modern presets (`NV_ENC_PRESET_P4_GUID`).
- Register D3D11 textures (`nvEncRegisterResource`) and map input buffers.
- Submit frames, lock bitstream buffers, extract H.264 NAL units into `EncodedPacket`.

### Phase 4: GPU Texture Pipeline & Color Conversion
- File: `engine/capture/include/mist/capture/ColorConverter.h` & `src/ColorConverter.cpp`
- Direct BGRA pass-through when supported by NVENC (`NV_ENC_BUFFER_FORMAT_ARGB`).
- D3D11 Video Processor or Compute Shader for hardware BGRA8 to NV12 color conversion.
- Bounded texture pool (2–3 textures) preventing GPU allocation on hot paths.
- Enforce strict bounded queue (max 2 frames), dropping stale frames if encoder lags.

### Phase 5: Synthetic Frame Generator
- File: `engine/capture/include/mist/capture/SyntheticFrameGenerator.h` & `src/SyntheticFrameGenerator.cpp`
- Generates moving GPU patterns directly onto D3D11 textures.
- Provides test frames for headless/CI validation without active desktop displays.

### Phase 6: Pipeline Integration & Telemetry
- File: `engine/capture/include/mist/capture/CaptureEncodePipeline.h` & `src/CaptureEncodePipeline.cpp`
- Coordinates capture thread and encoder worker.
- Extends telemetry to measure acquisition time, conversion time, encode time, capture-to-bitstream latency, FPS, and drops.

### Phase 7: Host CLI Diagnostics
- File: `apps/host/src/HostApplication.cpp` & `apps/host/src/main.cpp`
- Add `--list-displays`
- Add `--capture-test --display <idx> --duration <sec>`
- Add `--encode-test --codec h264 --width <w> --height <h> --fps <fps> --duration <sec> --output <path>`
- Add `--capture-encode-test --display <idx> --duration <sec> --report <path>`
- Update `--help` and `--self-test`.

### Phase 8: Testing & Verification
- Unit tests: configuration, display metadata, bitstream parsing, bounded queue behavior, telemetry calculations.
- Integration tests: D3D11 device creation, display enumeration, synthetic frame H.264 encoding, NVENC session initialization.
- Stability tests: 10-minute continuous capture-to-encode stress test.
- Execute full test suite via `scripts/verify-milestone-1.ps1`.

### Phase 9: Documentation & ADRs
- `docs/adr/0005-dxgi-desktop-duplication.md`
- `docs/adr/0006-nvenc-integration.md`
- `docs/adr/0007-gpu-texture-pipeline-and-synchronization.md`
- Updates to `docs/architecture.md` and `docs/testing.md`
- `docs/milestone-1-report.md`
