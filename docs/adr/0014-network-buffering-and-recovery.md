# ADR 0014: Low-Latency Network Buffering and Recovery Policy

## Status
Accepted

## Context
Traditional video streaming (e.g. YouTube, Netflix) uses multi-second playout jitter buffers to smooth network fluctuations. Remote interactive gaming cannot tolerate buffering; any queued frame introduces noticeable control lag. Mist's media pipeline requires a strict low-latency queuing policy across all network and decoding boundaries.

## Decision
1. **Bounded Host Transport Queue:**
   - Host queue between NVENC encoder and WebRTC track sender is strictly bounded to a maximum of 2 pending packets.
   - If network transmission stalls and the queue fills, the oldest frame is dropped immediately in favor of the freshest captured frame.
2. **Bounded Client Playout Queue:**
   - Client queue between WebRTC track receiver and Direct3D 11 hardware decoder is strictly bounded to a maximum of 2 pending frames.
   - Stale frames are discarded if the hardware decoder or presentation pipeline lags behind.
3. **Loss Recovery Without Buffering Bloat:**
   - On detected RTP packet loss or corrupted NAL unit sequences, the client does not stall presentation.
   - The client dispatches an RTCP Picture Loss Indication (PLI) to the host.
   - The host NVENC encoder responds by generating an instantaneous IDR keyframe (`forceKeyframeNext_ = true`), restoring pristine video stream state within one round-trip time.
4. **Zero Intermediate Disk Storage:**
   - All network packets, fragments, and depacketized access units are processed strictly in GPU/system RAM. No temporary `.h264` files are written to disk during live streaming.

## Consequences
- Guaranteed fresh frame delivery: latency never accumulates over time.
- Predictable memory footprint under all network conditions.
