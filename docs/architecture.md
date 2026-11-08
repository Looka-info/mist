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
| `engine/decode` | `mist_decode` | Direct3D 11 Hardware Video Decoder (`D3D11VideoDecoder`), Annex-B parser (`H264Parser`, `AnnexBReader`), DXGI device manager binding, and `DecodePresentPipeline`. | **Implemented & Verified** (M2) |
| `engine/render` | `mist_render` | Direct3D 11 Presentation Renderer (`D3D11Renderer`), DXGI Flip-Model swap chain (`DXGI_SWAP_EFFECT_FLIP_DISCARD`), D3D11 Video Processor NV12-to-RGB conversion, letterboxing. | **Implemented & Verified** (M2) |
| `engine/transport`| `mist_transport`| WebRTC peer connection manager, H.264 video track sender/receiver, SCTP data channels, RTCP PLI handling, transport statistics. | **Implemented & Verified** (M3) |
| `engine/audio` | `mist_audio` | Windows WASAPI loopback capture (`WasapiAudioCapture`), audio endpoint enumeration (`AudioDeviceEnumerator`), multi-channel ITU-R downmix & fractional resampler (`AudioResampler`), official Xiph Opus encoder/decoder (`OpusAudioEncoder`, `OpusAudioDecoder`), and client playout (`WasapiAudioPlayback`). | **Implemented & Verified** (M4) |
| `engine/input` | `mist_input` | Abstract input injection and polling contracts (`IInputManager`, input variant events). | **Interface Only** (Milestone 5) |
| `services` | `mist_services` | Signaling client interface (`ISignalingClient`, `SignalingMessage`, `WebSocketSignalingClient`) and Rust WebSocket signaling microservice (`mist-signaling`). | **Implemented & Verified** (M3) |
| `apps/host` | `mist_host` | Host executable entry point, CLI diagnostics (`--list-displays`, `--capture-test`, `--encode-test`, `--capture-encode-test`), self-test. | **Implemented & Verified** (M0/M1) |
| `apps/client` | `mist_client` | Client executable entry point, CLI diagnostics (`--decoder-info`, `--decode-test`, `--present-test`, `--decode-benchmark`), self-test. | **Implemented & Verified** (M0/M2) |
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
- **Milestone 2:** Client-Side Hardware Video Decoding (D3D11 Video MFT / D3D11VA) & Direct3D 11 Low-Latency Presentation (DXGI Flip-Model & Video Processor Blit). (**Complete & Hardware Verified**)
- **Milestone 3:** Native WebRTC Transport (DataChannels, SRTP Video Streaming, STUN/TURN, Rust WebSocket Signaling). (**Complete & Hardware Verified**)
- **Milestone 4:** Low-Latency Audio Streaming (WASAPI Loopback Capture, Opus 48kHz Codec, WebRTC RTP Audio Track, WASAPI Playback, A/V Sync). (**Complete & Hardware Verified**)
- **Milestone 5:** Low-Latency Input Streaming (Raw Input Polling, Virtual Gamepad, SendInput Injection).

---

## 6. WebRTC Transport & Signaling Architecture (Milestone 3)

### Core Components
1. **`libdatachannel v0.22.4` + `MbedTLS v3.6.2`**: Lightweight, C++20-compatible native WebRTC transport with DTLS-SRTP encryption without requiring external OpenSSL DLLs or Google `libwebrtc` monolithic binaries.
2. **`PeerConnectionManager`**: Encapsulates `rtc::PeerConnection`, ICE candidate gathering, offer/answer lifecycle, and queued candidate resolution ensuring remote candidates received before remote description are never dropped.
3. **`VideoTrackSender`**: RFC 6184 H.264 RTP packetizer with single NAL and FU-A fragmentation (1200 byte MTU), 90 kHz clock rate timestamp synchronization, and RTCP PLI (Picture Loss Indication) listener.
4. **`VideoTrackReceiver`**: RFC 6184 H.264 depacketizer reconstructing full in-memory Access Units and delivering them directly to the `D3D11VideoDecoder` (zero disk I/O). Dispatches RTCP PLI requests across the network on decoder recovery.
5. **`DataChannelManager`**: Manages SCTP DataChannels: reliable ordered channel for `session-control` and unordered low-latency channel for `diagnostics`.
6. **`FramePacer`**: Hybrid QPC sleep/spin pacer utilizing `timeBeginPeriod(1)` to enforce 60 FPS / 16.66 ms frame intervals with sub-millisecond precision.
7. **Rust WebSocket Signaling Server (`mist-signaling`)**: Ultra-lightweight asynchronous service authored in Rust using Tokio and Tungstenite, managing peer pairing, offer/answer routing, and ICE candidate exchange.

---

## 7. Architecture Decision Records (ADRs)

### ADR 0013: WebRTC Transport Implementation Strategy
- **Context:** Low-latency video streaming requires standard UDP-based transport, SRTP media encryption, ICE NAT traversal, and SCTP control channels. Full Google `libwebrtc` requires over 40 GB of toolchain and depot_tools, and is incompatible with clean CMake builds.
- **Decision:** Adopt `libdatachannel` compiled with `MbedTLS v3.6.2` (with `MBEDTLS_SSL_DTLS_SRTP`). FetchContent integrates cleanly into CMake with zero external runtime dependencies.
- **Consequences:** Minimal executable size footprint (< 4 MB added), full RFC compliance, standard WebRTC browser interoperability.

### ADR 0014: Zero Disk I/O In-Memory AU Piping and Bounded Queueing
- **Context:** Writing stream packets or access units to intermediate disk files degrades performance, introduces storage wear, and injects severe latency spikes.
- **Decision:** Pipe reassembled H.264 access units directly in memory from `VideoTrackReceiver` to `D3D11VideoDecoder`. Bounded queue depth of at most 2 frames; drop oldest frame immediately on backpressure.
- **Consequences:** Sustained sub-10ms network/decode pipeline latency, zero storage I/O on active streaming path.

### ADR 0015: Rust Tokio/Tungstenite Signaling Microservice
- **Context:** WebRTC session setup requires an out-of-band signaling mechanism for SDP exchange and ICE trickle.
- **Decision:** Implement `mist-signaling` as a standalone Rust microservice using `tokio` and `tokio-tungstenite`. The C++ engine communicates via `WebSocketSignalingClient`.
- **Consequences:** Memory-safe, high-concurrency, cross-platform signaling with zero thread contention in the native engine.

---

## 8. Audio Pipeline Architecture (Milestone 4)

```
[Host Audio Endpoint]
         │ (Master loopback audio)
         ▼
[WasapiAudioCapture] ──> (Event-driven shared loopback, silence padding)
         │ (Native Float32/Int16 PCM, arbitrary sample rate & channels)
         ▼
  [AudioResampler]   ──> (ITU-R BS.775 downmix, fractional linear resampler)
         │ (Normalized 48 kHz stereo Float32, 10 ms / 480-sample blocks)
         ▼
[OpusAudioEncoder]  ──> (xiph/opus v1.5.2, restricted low-delay, 128 kbps VBR, FEC)
         │ (Compressed Opus frames)
         ▼
 [AudioTrackSender]  ──> (RTP PT 111, SSRC 2, CNAME mist-stream, 48 kHz clock)
         │
═════════╪══════════════ [WebRTC DTLS-SRTP Transport] ═══════════════════════
         │
         ▼
[AudioTrackReceiver] ──> (RTP depacketizer, frame validation)
         │ (Opus frames)
         ▼
[OpusAudioDecoder]  ──> (48 kHz stereo decode, packet loss concealment / PLC)
         │ (Float32 stereo PCM)
         ▼
[WasapiAudioPlayback]──> (Bounded 30-80ms jitter buffer, event-driven render)
         │
         ▼
[Client Audio Endpoint] (Headphones / Speakers)
```

### Key Audio Design Properties
1. **Zero Virtual Driver Overhead:** Pure WASAPI shared loopback capture directly from default multimedia render endpoints.
2. **Deterministic Low Delay:** `OPUS_APPLICATION_RESTRICTED_LOWDELAY` eliminates lookahead filters, achieving ~92 µs average encoding time per 10 ms frame.
3. **Resilience & Continuity:** In-band FEC and native Opus PLC eliminate harsh audio dropouts; continuous silence padding prevents timestamp drift during quiet gameplay moments.
4. **Tight A/V Synchronization:** Shared RTCP CNAME `mist-stream` and capture-to-playout arrival tracking maintain p95 presentation skew below 20 ms (benchmark verified: 16.5 ms).

---

## 9. Audio Architecture Decision Records (ADRs)

### ADR 0016: WASAPI Loopback Audio Capture
- **Decision:** Use event-driven WASAPI shared loopback capture (`AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK`) with automatic mix format detection and zero-filled silence packet generation.

### ADR 0017: Opus Codec Integration and Low-Delay Profiling
- **Decision:** Statically integrate official `xiph/opus` v1.5.2 with `OPUS_APPLICATION_RESTRICTED_LOWDELAY`, 128 kbps VBR, 10 ms framing, and in-band Forward Error Correction (FEC).

### ADR 0018: Audio Resampling and Channel Downmixing Architecture
- **Decision:** Normalize all host audio configurations to 48,000 Hz stereo Float32 via ITU-R BS.775 multi-channel downmixing matrix and low-latency fractional linear interpolation.

### ADR 0019: WebRTC Audio Track and RTP Opus Framing
- **Decision:** Multiplex RFC 7587 Opus audio track (PT 111, SSRC 2, mid "audio") alongside H.264 video (SSRC 1) under unified RTCP CNAME `mist-stream`.

### ADR 0020: Client Audio Jitter Buffer and Packet Loss Concealment (PLC)
- **Decision:** Deploy a bounded 30 ms - 80 ms in-memory playout buffer in `WasapiAudioPlayback` with Opus PLC fallback to absorb network jitter without latency accumulation.

### ADR 0021: Audio/Video Synchronization and Skew Mitigation
- **Decision:** Correlate video Direct3D 11 present events and audio WASAPI playout events against a monotonic clock epoch, bounding p95 presentation skew to < 40 ms.
