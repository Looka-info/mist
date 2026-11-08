# Mist Milestone 4 Implementation Plan: Native WASAPI Audio Capture, Opus Encoding & WebRTC Audio Streaming

## 1. Executive Summary

Milestone 4 integrates native, low-latency, synchronized host system audio streaming into Mist. Remote players hearing live host game audio alongside the existing 1080p60 H.264 video stream is the core objective.
The implementation uses:
- Windows WASAPI shared-mode loopback capture for zero-copy digital audio acquisition directly from the active rendering device.
- Audio endpoint monitoring (`IMMNotificationClient`) and device enumeration.
- Format conversion and high-precision resampling to convert any device mix format (44.1k/48k/96k/192k, 16-bit/32-bit, mono/stereo/surround) to standard 48 kHz stereo PCM.
- Official Opus codec (`xiph/opus` v1.5.2) in `OPUS_APPLICATION_RESTRICTED_LOWDELAY` mode with in-band FEC and 10ms / 20ms framing.
- RFC 7587 WebRTC audio media track (Payload Type 111, SSRC 2) sharing RTCP CNAME `mist-stream` with video (SSRC 1) for lip-sync A/V correlation.
- Client-side Opus depacketization, decoding, Packet Loss Concealment (PLC), and bounded jitter buffer.
- Client WASAPI shared-mode playback.
- Diagnostic CLI tooling, automated tests, telemetry metrics, and full documentation.

---

## 2. Component Architecture

```
Host Audio Pipeline:
  [WASAPI Loopback Capture] (IAudioCaptureClient)
            │  PCM (Endpoint Mix Format: e.g. 48kHz Float32 / 44.1kHz Int16)
            ▼
  [AudioFormatConverter / Resampler] (Downmix to Stereo, Resample to 48kHz, Frame 10/20ms)
            │  PCM (48kHz Stereo Float32/Int16)
            ▼
  [OpusAudioEncoder] (libopus v1.5.2 RESTRICTED_LOWDELAY, 128kbps, FEC)
            │  Opus Access Unit
            ▼
  [AudioTrackSender] (rtc::OpusRtpPacketizer, PT 111, SSRC 2, CNAME mist-stream)
            │  RTP Packets
            ▼
  [WebRTC PeerConnection] (DTLS-SRTP Audio Media Track)

Client Audio Pipeline:
  [WebRTC PeerConnection]
            │  RTP Packets
            ▼
  [AudioTrackReceiver] (rtc::OpusRtpDepacketizer)
            │  Opus Packets (or NULL on packet loss)
            ▼
  [OpusAudioDecoder] (libopus decode + PLC synthesis)
            │  Decoded PCM (48kHz Stereo Float32)
            ▼
  [Jitter Buffer & Resynchronizer] (Bounded 20-40ms playout ring buffer)
            │
            ▼
  [WasapiAudioPlayback] (IAudioRenderClient, Event-driven shared mode)
```

---

## 3. Detailed Subsystem Specifications

### 3.1. Opus FetchContent Integration
- Fetch `xiph/opus` v1.5.2 via CMake `FetchContent`.
- Link static `opus` target to `mist_audio`.
- Enable C language in root `CMakeLists.txt` alongside CXX.

### 3.2. Audio Resampling and Channel Mixing (`AudioResampler.h`, `AudioResampler.cpp`)
- Converts arbitrary input configurations:
  - Sample rates: 44.1 kHz, 48 kHz, 88.2 kHz, 96 kHz, 192 kHz -> 48 kHz.
  - Channel layouts: Mono (duplicate to stereo), Stereo (pass-through), 5.1 & 7.1 Surround (ITU-R BS.775 downmix matrix).
  - Formats: IEEE 32-bit Float, 16-bit Signed Integer.
- Accumulates samples in a high-efficiency ring buffer to emit exact frame slices (480 samples for 10ms, 960 for 20ms).

### 3.3. Opus Encoder & Decoder (`OpusAudioEncoder`, `OpusAudioDecoder`)
- **Encoder:**
  - `OPUS_APPLICATION_RESTRICTED_LOWDELAY` to eliminate lookahead delay.
  - Default bitrate: 128 kbps (configurable 64–192 kbps).
  - In-band FEC enabled with configurable packet loss rate.
  - VBR enabled for optimal transient handling.
- **Decoder:**
  - 48 kHz stereo output.
  - Packet Loss Concealment (PLC) triggered by feeding `nullptr` on frame loss or sequence gaps.
  - In-band FEC decoding when lost packets are flagged.

### 3.4. WASAPI Audio Loopback Capture (`WasapiAudioCapture`)
- `IMMDeviceEnumerator` -> `eRender`, `eConsole` / `eMultimedia`.
- `IAudioClient` initialized with `AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK`.
- Event-driven worker thread querying `IAudioCaptureClient::GetBuffer`.
- Monitored by `IMMNotificationClient` for default endpoint migrations.
- Timestamps via `QueryPerformanceCounter` synchronized with host video frames.

### 3.5. WASAPI Audio Playback (`WasapiAudioPlayback`)
- Default render endpoint initialized in shared event-driven mode.
- Bounded jitter buffer (target 20–40 ms) absorbing network jitter while preventing buffer bloat.
- Underrun / overrun metrics tracking.

### 3.6. WebRTC Audio Streaming (`AudioTrackSender`, `AudioTrackReceiver`)
- `rtc::Description::Audio` media track.
- RFC 7587 Opus codec payload type 111, clock rate 48000.
- SSRC 2, CNAME `mist-stream`.
- Sender explicitly advances RTP timestamp by sample count (480 or 960).
- Receiver hooks `onTrack`, depacketizes Opus payloads, feeds decoder, and pushes to jitter buffer.

### 3.7. Audio/Video Synchronization
- Video track (SSRC 1) and audio track (SSRC 2) share RTCP CNAME `mist-stream`.
- Client calculates relative presentation skew:
  `skewMs = audioPresentationMs - videoPresentationMs`.
- Target: p95 skew within ±40 ms, <10 ms drift over 10 minutes.

### 3.8. Host and Client CLI Commands
- `mist-host --audio-devices`: Enumerate audio output devices.
- `mist-host --audio-test`: 3-second loopback capture test reporting RMS and peak levels.
- `mist-host --audio-encode-test`: Test capture + Opus compression.
- `mist-host --stream-live --audio`: Stream video + audio simultaneously.
- `mist-client --audio-devices`: Enumerate playback devices.
- `mist-client --audio-playback-test`: Test 440 Hz sine wave playback for 2 seconds.
- `mist-client --audio-receive-test`: Standalone audio-only WebRTC receiver.
- `mist-client --stream-receive --audio`: Receive and play synchronized video and audio.

---

## 4. Verification and Testing Plan
1. Unit tests:
   - `TestAudioFormat`: Verify format conversion, downmixing, and sample rate interpolation.
   - `TestOpusCodec`: Verify encode, decode, bitrate configuration, and PLC concealment.
   - `TestWasapiAudio`: Verify device enumeration and mock capture/render lifecycles.
   - `TestAudioWebRtc`: Verify audio track packetization, SDP generation, and RTP timestamp continuity.
2. End-to-end benchmark (`scripts/verify-milestone-4.ps1`):
   - 10-second live streaming session with `--audio`.
   - Verify video FPS >= 55, audio packet rate ~50 or 100 pps, 0 underruns, p95 A/V skew <= 40 ms.
   - Export results to `artifacts/milestone-4.json`.
