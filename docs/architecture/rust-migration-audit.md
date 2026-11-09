# Mist — Comprehensive Rust Migration Audit & Codebase Inventory

**Target Release:** v0.3.0  
**Baseline Commit:** `3c67de0c`  
**Active Development Branch:** `feature/advanced-streaming-expansion`  
**Classification Date:** November 9, 2026  

---

## 1. Executive Summary

Mist is transitioning from a C++20 desktop streaming prototype into a high-performance **Rust-first** native platform using **Slint 1.18.1** for UI presentation and native platform interfaces for ultra-low-latency real-time media.

This audit provides an inventory of all first-party C++ and Rust codebases, separating every module into:
1. **Category 1: Native Vendor / Hardware API Integrations** (preserved in native C/C++ or wrapped with Rust FFI).
2. **Category 2: Existing Media-Engine Functionality Awaiting Rust Migration** (target for future pure-Rust crates).
3. **Category 3: Application & Business Logic** (already migrated to or in progress of moving to Rust).
4. **Category 4: Diagnostic Test Harnesses & Tooling** (preserved for regression verification).

---

## 2. Component Inventory & Classification

### Category 1: Native Vendor & Hardware API Integrations (Preserved Native / FFI)
These components directly interact with hardware vendor SDKs, OS display servers, or kernel drivers where native C/C++ or COM interop is the standard platform convention:

| Component | Files / Paths | Why It Remains Native | FFI Boundary | Resource Lifetime & Ownership | Threading Model |
|---|---|---|---|---|---|
| **DXGI Capture Engine** | `engine/capture/src/DxgiCaptureEngine.cpp` | DXGI 1.2 Desktop Duplication COM API requires zero-overhead Direct3D 11 device contexts. | Bound to C++ media pipeline via `ICaptureEngine`. | Owns `IDXGIOutputDuplication` and staging textures; released on display switch or shutdown. | Dedicated high-frequency capture thread (60/120 Hz). |
| **WGC Capture Engine** | `engine/capture/src/WgcCaptureEngine.cpp` | Windows.Graphics.Capture WinRT COM API with borderless window tracking and D3D11 texture interop. | Bound via `ICaptureEngine`. | Owns `IGraphicsCaptureItem` and `Direct3D11CaptureFramePool`. | WinRT dispatch event thread. |
| **NVENC Hardware Encoder** | `engine/encode/src/NvencVideoEncoder.cpp` | Direct link to NVIDIA Video Codec SDK (`nvEncodeAPI.h`). | Called via `IVideoEncoder`. | Owns `NV_ENC_INITIALIZE_PARAMS`, registered D3D11 resource handles, bitstream buffers. | Synchronous encode on capture thread. |
| **D3D11VA Hardware Decoder** | `engine/decode/src/D3D11VideoDecoder.cpp` | Direct3D 11 Video Acceleration (`ID3D11VideoDecoder`, DXVA2) for zero-copy presentation. | Called via `IVideoDecoder`. | Owns D3D11 video decoder service, configuration profiles, output texture array. | Decode thread. |
| **D3D11 Renderer** | `engine/render/src/D3D11Renderer.cpp` | Native DirectX 11 swapchain (`IDXGISwapChain1`) with waitable object pacing for sub-frame latency. | C++ Render pipeline. | Owns swapchain, render target view, and backbuffer references. | Presentation thread synchronized to VSync. |
| **WASAPI Loopback Capture & Playback** | `engine/audio/src/WasapiAudio*.cpp` | Windows Core Audio WASAPI COM interfaces (`IAudioClient3`, `IAudioRenderClient`). | Audio pipeline. | Owns endpoint buffers; released on audio device disconnect. | Real-time MMCSS audio thread. |
| **IddCx Virtual Display Driver** | `platform/windows/src/VirtualDisplayManager.cpp` | Win32 IOCTL communication with Mist Virtual Display kernel driver (`IOCTL_IDD_*`). | C++ Platform API. | IOCTL handle open during headless hosting; closed on service termination. | Main host process. |

---

### Category 2: Media Engine Functionality Awaiting Rust Migration
These components implement protocol logic, parsers, algorithms, and session coordination that can be migrated to pure Rust or `webrtc-rs`:

| Component | Current Implementation | Migration Path | Target Rust Crate | Priority |
|---|---|---|---|---|
| **Bitstream Parsers** | `engine/decode/src/H264Parser.cpp`, `HevcParser.cpp`, `Av1Parser.cpp` | Pure algorithmic bitstream slice/header and LEB128 parsing. | `mist-bitstream` | Medium |
| **Annex B Reader** | `engine/decode/src/AnnexBReader.cpp` | NAL unit extraction and start-code scanning. | `mist-bitstream` | Medium |
| **Congestion Control & ABR** | `engine/transport/src/qos/AdaptiveBitrateController.cpp` | Google Congestion Control (GCC), REMB parsing, Kalman filtering. | `mist-transport-qos` | Medium |
| **Multi-Guest Fanout** | `engine/transport/src/MultiGuestFanout.cpp` | Thread-safe single-producer multi-consumer packet fanout queues. | `mist-session` | High |
| **Host Bandwidth Budget** | `engine/transport/src/qos/HostBandwidthBudgetManager.cpp` | Fair-share and priority-weighted uplink bandwidth allocator. | `mist-transport-qos` | Medium |
| **Cross-Platform Session** | `engine/decode/src/GuestClientSession.cpp` | Unified cross-platform client state machine. | `mist-client-core` | High |
| **HDR Tone Mapper** | `engine/capture/src/HdrToneMapper.cpp` | SMPTE ST 2084 PQ EOTF and gamut conversion math. | `mist-color` | Low |

---

### Category 3: Application & Business Logic (Already Migrated to Rust)
All core business logic, configuration, security, IPC, pairing, and desktop user interfaces now execute in pure, safe Rust:

| Crate / Application | Responsibility | Status |
|---|---|---|
| **`apps/mist-desktop`** | Main desktop application running **Slint 1.18.1** with Material Design 3 (Pixel Mint), multi-screen navigation, live host/guest controls, and controller bridge. | **100% Rust** |
| **`crates/mist-pairing`** | FIPS 180-4 SHA-256 tokens, DTLS certificate fingerprint extraction, Short Authentication String (SAS), rate limiting, replay defense, and approval lifecycles. | **100% Rust** |
| **`crates/mist-config`** | Central configuration schema, validation, bitrate clamping, JSON persistence. | **100% Rust** |
| **`crates/mist-domain`** | Domain models: `DeviceId`, `SessionId`, `DevicePermissions`, `GuestSession`. | **100% Rust** |
| **`crates/mist-ipc`** | Native Named Pipe binary frame protocol matching `0x4D495354` header layout. | **100% Rust** |
| **`crates/mist-platform-windows`** | Win32 platform integration, display enumeration, Wake-on-LAN magic packet dispatch. | **100% Rust** |
| **`crates/mist-telemetry`** | Session telemetry models, HUD degradation calculation, and metrics serialization. | **100% Rust** |
| **`services/signaling`** | Hardened WebSocket signaling server with token bucket rate limiting, IP room caps, and audit logs. | **100% Rust** |

---

### Category 4: Testing & Diagnostic Harnesses
| Harness | Target | Role |
|---|---|---|
| **`mist_unit_tests.exe`** | C++ Media Engine (230 test cases) | Exhaustive unit & integration tests covering DirectX, NVENC, WebRTC, FEC, and decoders. |
| **`cargo test --workspace`** | Rust Workspace (32 test cases) | Comprehensive verification of pairing cryptography, IPC binary frames, config, and signaling. |
| **`apps/client` & `apps/host`** | C++ Diagnostic CLI tools | Standalone smoke test executables for CI pipelines and headless validation (`--self-test`). |

---

## 3. Strict Boundary Rules

1. **Zero New Business Logic in C++**: All new application features, pairing rules, permission checks, UI controls, and user settings must be implemented exclusively in Rust.
2. **Deterministic Shutdown**: C++ media engine processes are spawned as child workers or dynamic libraries with strict lifetime bounds managed by the Rust orchestrator via Named Pipe IPC.
3. **No Raw Pointer Leakage**: Direct3D and COM pointers remain encapsulated behind RAII guards (`Microsoft::WRL::ComPtr`).
