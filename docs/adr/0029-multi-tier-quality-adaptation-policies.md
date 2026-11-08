# ADR 0029: Multi-Tier Quality Adaptation Policies

## Status
Accepted

## Context
Different gaming environments require distinct quality tradeoffs. Competitive esports titles prioritize minimum latency over visual fidelity, while story-driven cinematic games require pristine 1080p resolution. Furthermore, variable WAN connections require an automated adaptive policy.

## Decision
1. **Four Canonical Quality Profiles:**
   - **`Performance`:** 1280x720 @ 60 FPS, 3,500 kbps (1,500–6,000 kbps range), max acceptable RTT 45 ms, loss threshold 1.5%. Designed for competitive low-latency gaming.
   - **`Balanced`:** 1920x1080 @ 60 FPS, 8,000 kbps (3,000–14,000 kbps range), max acceptable RTT 60 ms, loss threshold 2.5%. Recommended default for standard LAN and broadband.
   - **`Quality`:** 1920x1080 @ 60 FPS, 15,000 kbps (6,000–25,000 kbps range), max acceptable RTT 80 ms, loss threshold 3.5%. Pristine visual fidelity on high-bandwidth links.
   - **`Adaptive`:** Dynamic 2,000–25,000 kbps governed by real-time AIMD congestion control.
2. **Prioritized Bandwidth Budgeting:**
   - Bandwidth is partitioned with strict priority:
     1. Input/Control (~50 kbps, Critical priority)
     2. Audio (128 kbps, High priority)
     3. Transport Headroom ($\approx 5\%$ of total, at least 150 kbps)
     4. Video (Remaining available bandwidth)
   - Ensures audio continuity and input responsiveness are never sacrificed to maintain video bitrate.

## Consequences
- Predictable and transparent performance characteristics across all network scenarios.
- Remote players hear uninterrupted audio even during severe video bitrate adaptation.
- Clear user-facing configuration in desktop settings.
