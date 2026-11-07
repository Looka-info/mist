# Mist Native Game Streaming Architecture

## 1. System Overview

Mist is a high-performance native Windows game-streaming platform designed to achieve ultra-low latency, 60+ FPS, 1080p remote interactive gaming. The platform is architected around explicit module boundaries, strict memory safety (RAII, explicit ownership), zero global mutable state, and platform isolation.

```
+-------------------------------------------------------------------------+
|                               MIST HOST                                 |
|                                                                         |
|  +-------------------+    +--------------------+    +----------------+  |
|  |   DXGI / WGC      |--->|   NVIDIA NVENC     |--->|  Native WebRTC |  |
|  | Desktop Capture   |    | Hardware Encoder   |    | Transport      |  |
|  +-------------------+    +--------------------+    +----------------+  |
|           ^                         ^                        |          |
|           |                         |                        |          |
|  +-------------------+    +--------------------+             |          |
|  | Windows Injection |<---| Session Controller |             |          |
|  | Keyboard/Mouse/Pad|    | (State / Auth)     |             |          |
|  +-------------------+    +--------------------+             |          |
+--------------------------------------------------------------|----------+
                                                               | Network
+--------------------------------------------------------------|----------+
|                                                              v          |
|  +-------------------+    +--------------------+    +----------------+  |
|  | Direct3D Present  |<---| Hardware Decoder   |<---| Transport      |  |
|  | Low-Latency Render|    | (NVDEC / D3D11VA)  |    | Demuxer        |  |
|  +-------------------+    +--------------------+    +----------------+  |
|           |                                                  ^          |
|           v                                                  |          |
|  +-------------------+                                       |          |
|  | Local Input Hook  |---------------------------------------+          |
|  | Low-Latency Poll  |                                                  |
|  +-------------------+                                                  |
|                               MIST CLIENT                               |
+-------------------------------------------------------------------------+
```

---

## 2. Module Boundaries & Responsibilities

The codebase is strictly separated into independent sub-libraries to ensure clean compile-time decoupling and testability:

| Directory | Target | Responsibility | Status |
|---|---|---|---|
| `engine/common` | `mist_common` | Type safety primitives (`Result<T>`, `Error`), structured thread-safe `Logger`, versioned `AppConfig` parser/validator. | **Implemented & Verified** (M0) |
| `engine/session` | `mist_session` | Session state machine (`SessionState`), lifecycle interface (`ISession`), observer notifications. | **Implemented & Verified** (M0) |
| `engine/telemetry` | `mist_telemetry` | Lock-free counters, gauges, bounded latency histograms, diagnostic metrics registry. | **Implemented & Verified** (M0/M1) |
| `engine/security` | `mist_security` | Constant-time string comparison, device identity contracts, auth token expiration, credential validation. | **Implemented & Verified** (M0) |
| `platform/windows`| `mist_platform_windows` | Win32/HRESULT error translation, D3D11 device creation, display enumeration, QPC timer, DPI awareness. | **Implemented & Verified** (M0/M1) |
| `engine/capture` | `mist_capture` | DXGI Desktop Duplication (`DxgiCaptureEngine`), GPU Synthetic Frame Generator, D3D11 Video Processor BGRA->NV12 Converter, and Bounded Pipeline. | **Implemented & Verified** (M1) |
| `engine/encode` | `mist_encode` | NVIDIA NVENC Hardware Video Encoder (`NvencVideoEncoder`), dynamic SDK loader, capability discovery, low-latency CBR tuning. | **Implemented & Verified** (M1) |
| `engine/decode` | `mist_decode` | Abstract video decoder contract (`IVideoDecoder`, `DecodedFrame`, `DecodeConfig`). | **Interface Only** (Milestone 2) |
| `engine/render` | `mist_render` | Abstract renderer contract (`IRenderer`, `RenderConfig`). | **Interface Only** (Milestone 2) |
| `engine/transport`| `mist_transport`| Abstract network transport contract (`ITransport`, `TransportChannel`). | **Interface Only** (Milestone 3) |
| `engine/audio` | `mist_audio` | Abstract WASAPI audio capture and playback contracts (`IAudioCapture`, `IAudioPlayback`). | **Interface Only** (Milestone 4) |
| `engine/input` | `mist_input` | Abstract input injection and polling contracts (`IInputManager`, input variant events). | **Interface Only** (Milestone 5) |
| `services` | `mist_services` | Signaling client interface (`ISignalingClient`, `SignalingMessage`). | **Interface Only** (Milestone 3) |
| `apps/host` | `mist_host` | Host executable entry point, CLI diagnostics (`--list-displays`, `--capture-test`, `--encode-test`, `--capture-encode-test`), self-test. | **Implemented & Verified** (M0/M1) |
| `apps/client` | `mist_client` | Client executable entry point, CLI arguments, configuration loading, lifecycle management, self-test. | **Implemented & Verified** (M0) |
| `apps/desktop` | `mist_desktop` | Unified desktop shell boundary. | **Implemented & Verified** (M0) |

---

## 3. Data Ownership & Resource Management

1. **Exclusive Ownership (`std::unique_ptr`):** Used for single-owner hardware abstractions (e.g. `ICaptureEngine`, `IVideoEncoder`).
2. **Shared State (`std::shared_ptr` / `std::weak_ptr`):** Used strictly for session observation patterns (`ISessionObserver`), ensuring observers can register and deregister without cyclic references or dangling pointers.
3. **Zero-Copy Memory Slices (`std::span`, `std::string_view`):** Used for packet buffers and read-only text parameters to eliminate needless memory allocations on hot paths.
4. **Value Semantics (`Result<T, Error>`):** Discriminated union mechanism (`std::variant`) enforcing explicit error checking at trust boundaries without exception overhead or C++23 dependencies.

---

## 4. Host & Client Responsibilities

### Host Application
- Discovers displays and GPUs.
- Captures desktop frames via low-overhead DXGI desktop duplication (Milestone 1).
- Encodes captured frames using NVIDIA NVENC hardware pipelines.
- Transmits encrypted video, audio, and control frames to the paired client.
- Injects validated remote keyboard, mouse, and gamepad commands into the host OS.

### Client Application
- Connects to signaling service and negotiates session pairing.
- Receives encoded network packets via WebRTC/UDP.
- Decodes video streams with hardware acceleration (NVDEC/D3D11VA).
- Renders frames via Direct3D swapchain with minimal buffering.
- Polls local raw input devices and dispatches events over the low-latency input transport channel.

---

## 5. Streaming Pipeline Roadmap

- **Milestone 0:** Foundation, project architecture, build system, error model, telemetry, and security contracts. (**Complete**)
- **Milestone 1:** Direct3D 11 GPU Capture (DXGI Desktop Duplication), GPU Color Conversion (D3D11 Video Processor), and Hardware Video Encoding (NVIDIA NVENC). (**Complete & Hardware Verified**)
- **Milestone 2:** Client-Side Hardware Video Decoding (NVDEC / D3D11VA) & Direct3D 11 Low-Latency Presentation.
- **Milestone 3:** Native WebRTC Transport (DataChannels, SRTP Video/Audio Streaming, STUN/TURN).
- **Milestone 4:** Low-Latency Audio Streaming (WASAPI Loopback Capture & Playback).
- **Milestone 5:** Low-Latency Input Streaming (Raw Input Polling, Virtual Gamepad, SendInput Injection).
