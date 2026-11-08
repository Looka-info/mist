# ADR 0020: Client Audio Jitter Buffer and Packet Loss Concealment (PLC)

## Status
Accepted

## Context
Network transit introduces packet delay variation (jitter) and burst packet loss. Without client buffering, audio playback would experience frequent buffer underruns, producing metallic crackling, clicks, or dropouts. Conversely, excessive buffering introduces unplayable audio latency that lags behind video presentation and user controller input.

## Decision
1. **Low-Latency Bounded Playout Buffer:**
   - Implement an in-memory jitter buffer in `WasapiAudioPlayback` storing decoded 48 kHz stereo Float32 PCM samples.
   - Target nominal playout buffer depth of 20 ms to 30 ms (2 to 3 Opus packets).
   - Enforce an upper bound limit of 80 ms (8 packets). If network congestion causes packets to queue beyond 80 ms, discard the oldest frames to immediately catch up to real-time playout without accumulating delay.
2. **Event-Driven WASAPI Playout:**
   - Configure `IAudioClient` in `AUDCLNT_SHAREMODE_SHARED` with `AUDCLNT_STREAMFLAGS_EVENTCALLBACK`.
   - Playout thread waits on the audio client's buffer event, filling available hardware buffer padding with queued decoded samples via `IAudioRenderClient::GetBuffer` and `ReleaseBuffer`.
3. **Packet Loss Concealment (PLC):**
   - When an expected Opus sequence number is missing and the playout buffer underruns, invoke `OpusAudioDecoder::decodeLostFrame()`.
   - The native Opus PLC engine synthesizes a frame matching the spectral parameters and pitch period of previous frames, fading out gracefully if loss persists.
   - Fill any residual hardware underruns with clean zeroed silence, preventing Windows audio driver glitch artifacts.
4. **Telemetry & Underrun Tracking:**
   - Record total audio frames decoded, audio packets received, playout buffer underruns, and dropped packets in real-time telemetry counters.

## Consequences
- Imperceptible audio playout latency (< 25 ms buffer delay) while effectively absorbing typical Wi-Fi and residential ISP jitter.
- High resilience to packet loss without audible clipping or popping artifacts.
- Deterministic memory usage bounded to < 100 KB RAM for audio playout buffering.
