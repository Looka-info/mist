# Mist Network QoS & Adaptive Streaming Architecture

## 1. Overview

Mist's Network Quality-of-Service (QoS) subsystem (`engine/transport/qos/`) ensures smooth, interactive 60 FPS remote gaming across volatile network connections. It provides media-path telemetry, sender-side congestion control, dynamic hardware encoder reconfiguration, and prioritized bandwidth budgeting.

```
                  +---------------------------------------------------+
                  |                 MIST HOST ENGINE                  |
                  |                                                   |
                  |  [NVENC Hardware Encoder]                         |
                  |          ▲ (Dynamic Reconfig)                     |
                  |          │                                        |
                  |  [AdaptiveBitrateController]                      |
                  |     ▲                ▲                            |
                  |     │                │                            |
                  |  [CongestionController] [QualityAdaptationManager]|
                  |  (AIMD State Machine)   (Profiles & Budgeting)    |
                  |          ▲                                        |
                  |          │                                        |
                  |  [NetworkQualityEstimator] (EWMA Smoothing)       |
                  |          ▲                                        |
                  |          │                                        |
                  |  [VideoTrackSender] (rtc::RembHandler)            |
                  +----------│----------------------------------------+
                             │ RTCP REMB / RR / PLI
                             │ (WebRTC DTLS-SRTP)
                  +----------│----------------------------------------+
                  |  [VideoTrackReceiver]                             |
                  |  (rtc::RtcpReceivingSession)                      |
                  |                                                   |
                  |                 MIST CLIENT ENGINE                |
                  +---------------------------------------------------+
```

---

## 2. Telemetry Collection & Smoothing (`NetworkQualityEstimator`)

Metrics are updated upon receiving RTCP Receiver Reports and REMB packets:
- **Round-Trip Time (RTT):** Smoothed via Exponential Weighted Moving Average (EWMA, $\alpha = 0.15$):
  $$\text{RTT}_{\text{smoothed}} = 0.85 \cdot \text{RTT}_{\text{prev}} + 0.15 \cdot \text{RTT}_{\text{sample}}$$
- **Packet Loss Percentage:** Derived from RTCP fraction lost (0..255) and smoothed via EWMA ($\alpha = 0.20$):
  $$\text{Loss}_{\text{sample}} = \frac{\text{fractionLost}}{256} \times 100\%$$
  $$\text{Loss}_{\text{smoothed}} = 0.80 \cdot \text{Loss}_{\text{prev}} + 0.20 \cdot \text{Loss}_{\text{sample}}$$
- **Jitter:** Sampled from RTCP interarrival jitter and converted from 90 kHz clock ticks to milliseconds.
- **Estimated Bandwidth:** Directly updated from incoming RTCP REMB messages or measured delivered throughput.
- **Connection Type:** Determined from `pc_->getSelectedCandidatePair(&local, &remote)`:
  - `DirectLan`: Local and remote candidate types are `host`.
  - `DirectWan`: Local or remote candidate types are `srflx` or `prflx`.
  - `RelayedTurn`: Local or remote candidate types are `relay`.

---

## 3. Congestion-Control State Machine (`CongestionController`)

The controller transitions across 5 operational states:

```
                  +-----------------------------+
                  |           STABLE            |
                  +-----------------------------+
                     │                       ▲
       No Loss &     │                       │ 2000ms Stable
       Good RTT      ▼                       │
                  +-----------------------------+
                  |         INCREASING          |
                  +-----------------------------+
                     │
       Loss >= 2.5%  │
       or RTT Spike  ▼
                  +-----------------------------+
                  |          CONGESTED          |
                  +-----------------------------+
                     │                       ▲
       Loss >= 15%   │                       │ Backoff Cooldown
       or RTT x2.5   ▼                       │ (1000ms)
                  +-----------------------------+
                  |     CRITICALLY CONGESTED    |
                  +-----------------------------+
                     │                       ▲
                     └─────────► RECOVERING ─┘
```

### Rate Adaptation Rules
1. **Additive Increase (`Increasing`):**
   - Evaluated every 500 ms when loss is zero and RTT is stable.
   - Bitrate increases by $+5\%$ or $+250$ kbps:
     $$R_{\text{next}} = \min(R_{\text{max}}, R_{\text{current}} + \max(250, R_{\text{current}} / 20))$$
   - Clamped to REMB ceiling if available.
2. **Multiplicative Decrease (`Congested`):**
   - Triggered when loss $\ge 2.5\%$ or $\text{RTT} > \text{RTT}_{\text{max}}$.
   - Backs off bitrate by $20\%$ ($x0.80$):
     $$R_{\text{next}} = \max(R_{\text{min}}, R_{\text{current}} \times 0.80)$$
   - Minimum 1000 ms cooldown between consecutive backoffs prevents oscillation.
3. **Emergency Cut (`CriticallyCongested`):**
   - Triggered when loss $\ge 15.0\%$ or $\text{RTT} > 2.5 \times \text{RTT}_{\text{max}}$.
   - Cuts bitrate by $50\%$ down to minimum configured floor.

---

## 4. Bandwidth Budgeting (`QualityAdaptationManager`)

Available network capacity is budgeted with strict priority:

| Stream Component | Priority | Bandwidth Allocation | Purpose |
|---|---|---|---|
| **Remote Input / Control** | Critical (1) | 50 kbps | Ensures keyboard, mouse, and gamepad responsiveness is never throttled. |
| **System Audio** | High (2) | 128 kbps | Preserves fullband Opus stereo sound without clipping or dropouts. |
| **Transport Headroom** | Reserve (3) | $\max(150 \text{ kbps}, 5\% \text{ total})$ | Absorbs network packet jitter and bursts. |
| **Video Stream** | Dynamic (4) | Remainder | Allocated to NVENC H.264 video. |

---

## 5. Dynamic NVENC Reconfiguration (`NvencVideoEncoder`)

- Invokes `nvEncReconfigureEncoder` with `NV_ENC_RECONFIGURE_PARAMS`.
- Reconfigures CBR `averageBitRate` and `maxBitRate` in-flight without destroying the Direct3D 11 device, encoder session, or allocated bitstream buffers.
- Threshold gating: Reconfiguration is executed only when $\Delta R \ge 250$ kbps or during critical congestion state transitions.
