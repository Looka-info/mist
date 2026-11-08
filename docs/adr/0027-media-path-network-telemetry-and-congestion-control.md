# ADR 0027: Media-Path Network Telemetry and Congestion Control

## Status
Accepted

## Context
Interactive gaming requires continuous, low-latency video streaming without bufferbloat or frame queuing. Traditional TCP congestion control or SCTP DataChannel congestion controls reflect transport layer characteristics of control channels, not the high-throughput UDP/RTP media pipeline. The video stream needs sender-side congestion estimation derived from media-path feedback (RTCP Receiver Reports, REMB, RTT, and loss measurements).

## Decision
1. **Media-Path Feedback Integration:**
   - Audit `libdatachannel v0.22.4`: verify availability of `rtc::RembHandler` (sender) and `rtc::RtcpReceivingSession` (receiver).
   - Chain `rtc::RembHandler` into `VideoTrackSender` to process incoming client REMB bitrates.
   - Chain `rtc::RtcpReceivingSession` into `VideoTrackReceiver` to generate Receiver Reports and REMB packets without third-party frameworks.
2. **Explicit Congestion State Machine:**
   - Implement `CongestionController` with 5 discrete states:
     - `Stable`: Network operating within nominal bandwidth; normal delivery.
     - `Increasing`: Probing capacity upwards via Additive Increase (+5% or +250 kbps per 500ms period).
     - `Congested`: Packet loss >= threshold (2.5%) or RTT inflation; triggers Multiplicative Decrease (x0.80).
     - `Recovering`: Post-backoff stabilization period (2000ms hold).
     - `CriticallyCongested`: Severe loss (>= 15%) or RTT blowup (> 2.5x max RTT); immediate 50% cut to minimum floor.
3. **Hysteresis & Anti-Oscillation:**
   - Impose a minimum 1000ms cooldown (`kMinBackoffCooldownMs`) between consecutive backoffs to prevent rapid oscillation.
   - Distinguish isolated single packet drops from sustained congestion using exponential weighted moving averages (EWMA, $\alpha = 0.20$).

## Consequences
- Prevents bufferbloat and network queuing on constrained paths.
- Responds within 1 RTT to severe congestion while avoiding panic drops on isolated losses.
- Bounded video bitrate strictly between configured profile minimum and maximum.
