# ADR 0028: Adaptive Video Bitrate with NVENC Dynamic Reconfiguration

## Status
Accepted

## Context
When network bandwidth shifts dynamically during game streaming, the video encoder must adapt its output bitrate without dropping the streaming session, resetting the GPU context, or introducing visual frame glitches. Tearing down and recreating the NVIDIA NVENC session incurs 50–150 ms delays and causes dropped frames.

## Decision
1. **Dynamic In-Session Reconfiguration:**
   - Use NVENC's native `nvEncReconfigureEncoder` API with `NV_ENC_RECONFIGURE_PARAMS`.
   - Update `averageBitRate` and `maxBitRate` in `NV_ENC_CONFIG::rcParams` on the active encoder session handle without destroying the session.
2. **Delta Threshold Gating:**
   - Bitrate adjustments are evaluated periodically (every 500ms).
   - Reconfiguration is only dispatched to the hardware encoder if the recommended bitrate delta $\Delta R \ge 250$ kbps, or if transitioning to/from urgent congestion states (`Congested` / `CriticallyCongested`).
   - Prevents GPU command queue flooding during minor bandwidth fluctuations.
3. **Preservation of Frame Ordering:**
   - Encoder reconfiguration occurs under encoder mutex locking between picture encodes, ensuring proper monotonic frame counter progression and zero bitstream corruption.

## Consequences
- Seamless in-flight bitrate adaptation without encoder reinitialization.
- Zero frame drops or pipeline stalls during network adaptation.
- Sub-millisecond reconfiguration execution time.
