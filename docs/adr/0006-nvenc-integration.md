# ADR 0006: NVIDIA NVENC Hardware Video Encoding Integration

## Status
Accepted

## Context
High-performance native game streaming requires low-overhead hardware video encoding capable of sub-frame latencies (less than 8ms per frame at 60 FPS). Software encoders (e.g., x264) introduce substantial CPU usage and latency jitter.

## Decision
1. **Dynamic SDK Integration:** Interface with the official NVIDIA Video Codec SDK via dynamic library loading (`LoadLibraryA("nvEncodeAPI64.dll")` and `NvEncodeAPICreateInstance`). This ensures binary portability, eliminates static library linking risks, and allows clean runtime capability detection and fallbacks.
2. **Modern Tuning and Presets:** Configure the encoder with the modern low-latency P4 preset (`NV_ENC_PRESET_P4_GUID`), Ultra-low-latency tuning (`NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY`), and constant bitrate (CBR) rate control mode with zero B-frames (pure IPPP sequence) and periodic IDR frames (`repeatSPSPPS = 1`).
3. **Shared Direct3D 11 Device Context:** Initialize the NVENC encode session using the identical `ID3D11Device` that manages capture and color conversion, avoiding inter-device synchronization overhead and cross-adapter copies.
4. **Input Resource Registration Caching:** Cache registered Direct3D 11 texture resources in an internal lookup map (`std::unordered_map<void*, RegisteredResource>`). Textures are registered once via `nvEncRegisterResource`, mapped per-frame via `nvEncMapInputResource`, and unmapped after submission, completely eliminating per-frame driver registration overhead.
5. **Pre-allocated Bitstream Buffers:** Pre-allocate a bounded ring of output bitstream buffers (3 slots) during initialization, guaranteeing zero runtime dynamic allocations during frame encoding.

## Consequences
- High-efficiency hardware encoding on NVIDIA Turing/Ampere/Ada GPUs (measured ~2.2 ms - 4.1 ms p50 on GTX 1660 SUPER).
- Zero CPU texture readbacks: frames remain resident in GPU VRAM from capture through bitstream retrieval.
- Clean resource teardown and lifecycle management.
