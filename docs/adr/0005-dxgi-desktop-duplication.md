# ADR 0005: DXGI Desktop Duplication for Low-Latency Screen Capture

## Status
Accepted

## Context
Mist targets low-latency 60+ FPS game streaming on Windows. Traditional screen capture methods (such as GDI `BitBlt` or Windows Graphics Capture API without hardware interop) suffer from high CPU consumption, memory copies between system memory and GPU VRAM, or variable frame delivery jitter.

## Decision
1. **API Selection:** Use the DirectX Graphics Infrastructure (DXGI) Desktop Duplication API (`IDXGIOutputDuplication`) paired with a Direct3D 11 device created on the adapter driving the target display.
2. **GPU-Resident Pipeline:** Acquired desktop surfaces (`IDXGIResource`) are copied entirely on the GPU into a dedicated shared `ID3D11Texture2D` using `ID3D11DeviceContext::CopyResource`. The texture is configured with `D3D11_USAGE_DEFAULT`, `CPUAccessFlags = 0`, and `BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE`, guaranteeing zero CPU readback.
3. **RAII Lifetime Management:** Every call to `AcquireNextFrame` is strictly paired with `ReleaseFrame`. Acquired duplication surfaces are never accessed outside their acquired lifetime.
4. **Resilience & Error Handling:**
   - `DXGI_ERROR_WAIT_TIMEOUT`: Treated as a normal no-frame condition; recorded in telemetry without error escalation.
   - `DXGI_ERROR_ACCESS_LOST`: Automatically triggers duplication interface teardown and re-initialization (handling mode switches, UAC prompts, or resolution changes).
   - `E_ACCESSDENIED (0x80070005)`: Transparently identified for headless or non-interactive sessions, routing test and diagnostic runs gracefully to the GPU synthetic frame generator.

## Consequences
- True zero-copy GPU capture pipeline.
- Sub-millisecond texture acquisition and copy overhead.
- Robust recovery across display mode changes and secure desktop transitions.
