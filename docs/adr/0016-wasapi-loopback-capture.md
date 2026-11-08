# ADR 0016: WASAPI Loopback Audio Capture

## Status
Accepted

## Context
High-performance remote game streaming requires capturing the host machine's master audio output (game sounds, dialogue, sound effects) with minimal latency and zero invasive kernel drivers or virtual audio cables. On Windows, the native subsystem for capturing rendered output is the Windows Audio Session API (WASAPI) in loopback mode.

## Decision
1. **WASAPI Shared Mode Loopback:**
   - Initialize `IAudioClient` with `AUDCLNT_SHAREMODE_SHARED` and stream flag `AUDCLNT_STREAMFLAGS_LOOPBACK`.
   - Capture from the default multimedia render endpoint (`eRender`, `eMultimedia`) enumerated via `IMMDeviceEnumerator`.
2. **Event-Driven Non-Blocking Architecture:**
   - Configure event-driven buffer signaling using `AUDCLNT_STREAMFLAGS_EVENTCALLBACK` and `SetEventHandle`.
   - Run capture in a dedicated high-priority capture thread waiting on Win32 event handles (`WaitForSingleObject`) rather than polling loops, minimizing CPU utilization and context switches.
3. **Format Auto-Discovery:**
   - Query the host audio endpoint's native mix format using `IAudioClient::GetMixFormat`.
   - Accommodate diverse hardware formats (Float32 or Int16, sample rates ranging from 44.1 kHz to 192 kHz, 2.0 stereo to 7.1 surround) and forward the stream to a unified resampling stage.
4. **Silence Flag Handling:**
   - Inspect `dwFlags` returned by `IAudioCaptureClient::GetBuffer`.
   - When `AUDCLNT_BUFFERFLAGS_SILENT` is asserted (host playing silence/idling), generate explicit zero-filled PCM buffers to maintain continuous, uninterrupted Opus framing and avoid clock drift or receiver underruns.

## Consequences
- Zero kernel drivers or third-party virtual audio devices required.
- Near-zero host capture overhead (< 0.5% CPU utilization).
- Robust timeline continuity even when the host game or OS pauses audio playback.
