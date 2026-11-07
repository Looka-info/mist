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

| Directory | Target | Responsibility | Milestone 0 Status |
|---|---|---|---|
| `engine/common` | `mist_common` | Type safety primitives (`Result<T>`, `Error`), structured thread-safe `Logger`, versioned `AppConfig` parser/validator. | **Implemented & Verified** |
| `engine/session` | `mist_session` | Session state machine (`SessionState`), lifecycle interface (`ISession`), observer notifications. | **Implemented & Verified** |
| `engine/telemetry` | `mist_telemetry` | Lock-free counters, gauges, bounded latency histograms, diagnostic metrics registry. | **Implemented & Verified** |
| `engine/security` | `mist_security` | Constant-time string comparison, device identity contracts, auth token expiration, credential validation. | **Implemented & Verified** |
| `platform/windows`| `mist_platform_windows` | Win32 error translation (`GetLastError` to `Error`), HRESULT translation, high-resolution QPC timer, DPI awareness. | **Implemented & Verified** |
| `engine/capture` | `mist_capture` | Abstract capture contract (`ICaptureEngine`, `CapturedFrame`, `CaptureConfig`). | **Interface Only** (No DXGI) |
| `engine/encode` | `mist_encode` | Abstract video encoder contract (`IVideoEncoder`, `EncodedPacket`, `EncodeConfig`). | **Interface Only** (No NVENC) |
| `engine/decode` | `mist_decode` | Abstract video decoder contract (`IVideoDecoder`, `DecodedFrame`, `DecodeConfig`). | **Interface Only** (No NVDEC) |
| `engine/render` | `mist_render` | Abstract renderer contract (`IRenderer`, `RenderConfig`). | **Interface Only** (No D3D) |
| `engine/transport`| `mist_transport`| Abstract network transport contract (`ITransport`, `TransportChannel`). | **Interface Only** (No WebRTC) |
| `engine/audio` | `mist_audio` | Abstract WASAPI audio capture and playback contracts (`IAudioCapture`, `IAudioPlayback`). | **Interface Only** (No WASAPI) |
| `engine/input` | `mist_input` | Abstract input injection and polling contracts (`IInputManager`, input variant events). | **Interface Only** (No SendInput) |
| `services` | `mist_services` | Signaling client interface (`ISignalingClient`, `SignalingMessage`). | **Interface Only** (No WebSockets) |
| `apps/host` | `mist_host` | Host executable entry point, CLI arguments, configuration loading, lifecycle management, self-test. | **Implemented & Verified** |
| `apps/client` | `mist_client` | Client executable entry point, CLI arguments, configuration loading, lifecycle management, self-test. | **Implemented & Verified** |
| `apps/desktop` | `mist_desktop` | Unified desktop shell boundary. | **Implemented & Verified** |

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

## 5. Future Streaming Pipeline (Milestones 1–5)

- **Milestone 1:** Direct3D 11 / DXGI Desktop Capture & Native Presenter.
- **Milestone 2:** NVIDIA NVENC Video Encoding (H.264 / HEVC) & Hardware Decoding.
- **Milestone 3:** Low-latency WebRTC DataChannel & Video Transport pipeline.
- **Milestone 4:** WASAPI Audio Loopback Capture & Playback.
- **Milestone 5:** Low-latency Windows Input Injection & Virtual Controller Emulation.
