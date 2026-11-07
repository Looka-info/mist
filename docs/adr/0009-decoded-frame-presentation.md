# ADR 0009: Decoded Frame Direct3D 11 Presentation & Flip-Model Scheduling

## Status
Accepted

## Context
High-performance remote interactive gaming requires smooth, low-latency video presentation. Display presentation on Windows must minimize display jitter, prevent tearing, avoid blocking the decode/network pipelines, and handle dynamic window resizes and fullscreen toggles cleanly.

## Decision

1. **DXGI Flip-Model Swap Chain:**
   - Use `DXGI_SWAP_EFFECT_FLIP_DISCARD` with a 2-buffer (double-buffered) swap chain.
   - Flip model bypasses the Windows Desktop Window Manager (DWM) redirection surface copy, significantly reducing latency and GPU memory bandwidth compared to legacy `DXGI_SWAP_EFFECT_DISCARD` or `DXGI_SWAP_EFFECT_SEQUENTIAL`.
   - Query and enable `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` for variable refresh rate (G-Sync/FreeSync) displays where available.

2. **Decoded Frame Color Blit & Aspect Ratio:**
   - Render decoded NV12 frames onto the RGB backbuffer using the GPU (Direct3D 11 Video Processor or HLSL pixel shader quad).
   - Calculate viewports dynamically to preserve stream aspect ratio with black letterboxing or pillarboxing when window dimensions differ from stream resolution.

3. **Decoupled Presentation Queue & Drop Policy:**
   - The presentation worker runs on an independent scheduling thread decoupled from the hardware decoder.
   - Decoded frames are placed in a bounded queue capped at a maximum of 2 pending frames.
   - When the renderer is scheduled and multiple frames are queued, obsolete frames are dropped in favor of the freshest available frame (`framesDroppedPresentation++`). In low-latency remote gaming, displaying the newest frame immediately is strictly favored over presenting delayed frames.

4. **Monotonic Presentation Telemetry:**
   - Frame timing is tracked using high-resolution monotonic timestamps (`QueryPerformanceCounter`).
   - Latencies are recorded from encoded packet arrival to decode completion, and from decode completion to actual `IDXGISwapChain::Present` submission.

## Consequences
- Ultra-low presentation overhead (sub-1ms presentation submission).
- Clean handling of window resizes, minimizing D3D11 swapchain recreation stalls.
- Zero tearing and optimal latency with modern Windows 11 DWM.
