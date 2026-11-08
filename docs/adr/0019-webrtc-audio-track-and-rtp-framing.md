# ADR 0019: WebRTC Audio Track and RTP Opus Framing

## Status
Accepted

## Context
Transmitting Opus-encoded audio concurrently with H.264 video over WebRTC requires standard-compliant RTP packetization, track negotiation, and multiplexing across a single DTLS/ICE peer connection without interference between media modalities.

## Decision
1. **RFC 7587 Opus RTP Profile:**
   - Configure WebRTC audio track with standard dynamic Payload Type 111.
   - SSRC assigned to 2 (host audio) while video uses SSRC 1.
   - Set RTP clock rate to 48,000 Hz, advancing timestamps monotonically by exactly 480 ticks per 10 ms frame (or 960 ticks per 20 ms frame).
2. **Track Identification & Multiplexing:**
   - Both audio and video media tracks belong to the identical RTCP Canonical Name (CNAME) `mist-stream` and stream ID `mist-av-stream`.
   - Audio track MID configured as `"audio"`; video MID configured as `"video"`.
   - In `WebRtcTransport`, hook the PeerConnection `onTrack` handler and dispatch tracks based on description type (`track->description().type() == "audio"` vs `"video"`), attaching the corresponding `AudioTrackReceiver` or `VideoTrackReceiver`.
3. **Low-Latency Transport Queue:**
   - Packetize Opus payloads via `rtc::OpusRtpPacketizer` directly onto the media track.
   - Limit transport buffering to prevent bufferbloat under packet loss or jitter spikes; if the send queue fills, discard stale frames to preserve interactive freshness.

## Consequences
- Full interoperability with standard WebRTC implementations and media inspectors.
- Native synchronization basis established at the RTP layer via CNAME grouping.
- Clean separation between audio and video network handlers with unified peer connection teardown.
