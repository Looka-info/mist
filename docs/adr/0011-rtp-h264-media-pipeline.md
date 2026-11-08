# ADR 0011: H.264 RTP Packetization and Media Pipeline

## Status
Accepted

## Context
Mist captures desktop frames on GPU hardware, converts color spaces via Direct3D 11 Video Processors, and encodes frames into H.264 Annex-B bitstreams with NVIDIA NVENC. These bitstreams must be transmitted over WebRTC using standard RTP packetization without intermediate transcode or disk caching.

### Key Technical Challenges:
1. **Packetization Mode:** RFC 6184 defines Single NAL Unit Mode, Non-Interleaved Mode, and Interleaved Mode. Interactive gaming requires Non-Interleaved Mode (mode 1) to allow Fragmentation Units (FU-A) without interleaving overhead.
2. **MTU & Fragmentation:** Encoded video frames regularly exceed standard Ethernet MTU (1500 bytes). Large video slices must be fragmented into FU-A packets with Start (S) and End (E) bit markers, preserving decoding order. Small parameter sets (SPS, PPS) and AUDs fit within Single NAL Unit packets.
3. **Clock Frequency:** Standard video RTP clock frequency is 90,000 Hz (90 kHz). Wall-clock timestamps from host QueryPerformanceCounter (QPC) must be mapped accurately into 90kHz RTP timestamps to prevent presentation jitter.
4. **Keyframe Recovery:** Packet loss can corrupt P-frames. The client must signal Picture Loss Indication (PLI) via RTCP, prompting the host encoder to generate an immediate IDR keyframe.

## Decision
1. Use `rtc::H264RtpPacketizer` configured with `Separator::LongStartSequence` and a 90kHz clock.
2. Chain `rtc::RtcpSrReporter` for periodic Sender Reports and `rtc::RtcpNackResponder` for standard NACK retransmission support.
3. Enforce non-interleaved packetization mode in SDP negotiation (`packetization-mode=1;profile-level-id=42e01f`).
4. On the client, use `rtc::Track::onFrame` (backed by `H264RtpDepacketizer`) to reassemble fragmented NALUs into memory buffers with Annex-B start codes, directly feeding `D3D11VideoDecoder`.
5. Register RTCP feedback handlers on the host track to trigger `NvencVideoEncoder::requestKeyframe()` upon receipt of PLI / FIR packets.

## Consequences
- Guaranteed RFC 6184 compliance and standard WebRTC browser interoperability.
- Sub-millisecond packetization overhead with zero CPU memory copies of raw pixel buffers.
- Rapid recovery from burst packet loss on noisy networks.
