# Mist Real-Time Media & Signaling Protocol Specification

## 1. Protocol Architecture Overview

Mist employs a multi-tiered real-time protocol architecture combining out-of-band WebSocket signaling, standard WebRTC DTLS-SRTP encrypted media transport, and SCTP application data channels.

```
                      +-----------------------------+
                      | WebSocket Signaling (JSON)  |
                      |  Default TCP Port 8080      |
                      +-----------------------------+
                                     │
                 ┌───────────────────┴───────────────────┐
                 ▼                                       ▼
    +-------------------------+             +-------------------------+
    |       MIST HOST         |             |       MIST CLIENT       |
    +-------------------------+             +-------------------------+
                 │                                       │
                 ▼                                       ▼
  +─────────────────────────────────────────────────────────────────────+
  |              WebRTC PeerConnection (DTLS 1.2+ / SRTP)               |
  |  - Video Track: RFC 6184 H.264 (PT 96, SSRC 1, 90 kHz clock)        |
  |  - Audio Track: RFC 7587 Opus (PT 111, SSRC 2, 48 kHz clock)        |
  |  - RTCP Feedback: RFC 4585 PLI & Unified CNAME `mist-stream`        |
  |  - SCTP DataChannel: `session-control` (Reliable Ordered)           |
  |  - SCTP DataChannel: `diagnostics` (Unreliable Unordered)           |
  +─────────────────────────────────────────────────────────────────────+
```

---

## 2. Signaling Protocol (WebSocket / JSON)

The signaling microservice (`bin/mist-signaling.exe`) listens on TCP port 8080 and brokers session coordination between host and client. All messages are encoded in UTF-8 JSON.

### Message Formats

#### 2.1 Registration (`register`)
```json
{
  "type": "register",
  "session_id": "mist-session-default",
  "role": "host",
  "token": "mist-dev-token"
}
```

#### 2.2 Registration Confirmation (`registered`)
```json
{
  "type": "registered",
  "role": "host",
  "peer_connected": false
}
```

#### 2.3 Peer Notification (`peer_joined` / `peer_left`)
```json
{
  "type": "peer_joined",
  "role": "client"
}
```

#### 2.4 SDP Offer (`offer`)
```json
{
  "type": "offer",
  "session_id": "mist-session-default",
  "sdp": "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=video 9 UDP/TLS/RTP/SAVPF 96\r\n..."
}
```

#### 2.5 SDP Answer (`answer`)
```json
{
  "type": "answer",
  "session_id": "mist-session-default",
  "sdp": "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\nm=video 9 UDP/TLS/RTP/SAVPF 96\r\n..."
}
```

#### 2.6 Trickle ICE Candidate (`candidate`)
```json
{
  "type": "candidate",
  "session_id": "mist-session-default",
  "candidate": "a=candidate:1 1 UDP 2122317823 192.168.1.9 56151 typ host",
  "mid": "video"
}
```

---

## 3. Video Media Encapsulation (RFC 6184 H.264)

### Parameters
- **Payload Type:** 96 (Dynamic)
- **SSRC:** 1
- **Clock Rate:** 90,000 Hz (90 kHz standard)
- **Media Line:** `m=video 9 UDP/TLS/RTP/SAVPF 96`
- **MID:** `"video"`

### Packetization Scheme
1. **Single NAL Unit Packet:** When the complete NAL unit (including header) does not exceed MTU (1200 bytes), it is transmitted as a single RTP packet.
2. **Fragmentation Unit A (FU-A):** When an encoded slice exceeds MTU:
   - Split across sequential FU-A packets.
   - FU indicator byte: `F` bit, `NRI` bits, type 28 (`FU-A`).
   - FU header byte: `S` (start bit), `E` (end bit), `R` (reserved 0), 5-bit original NAL unit type.

---

## 4. Audio Media Encapsulation (RFC 7587 Opus)

### Parameters
- **Payload Type:** 111 (Dynamic standard for Opus)
- **SSRC:** 2
- **Clock Rate:** 48,000 Hz
- **Channels:** 2 (Stereo)
- **Framing Duration:** 10 ms (480 samples) default; 20 ms (960 samples) supported
- **Media Line:** `m=audio 9 UDP/TLS/RTP/SAVPF 111`
- **MID:** `"audio"`

### RTP Timestamp Progression
The RTP timestamp progresses at exactly 48,000 Hz. For 10 ms audio frames:
$$\Delta \text{Timestamp} = 480 \text{ ticks per packet}$$

### Payload Header & In-Band FEC
Opus packets contain TOC (Table of Contents) byte and compressed frame data. When forward error correction is enabled (`OPUS_SET_INBAND_FEC(1)`), each packet contains redundant recovery data for the preceding frame, decoded via `opus_decode(..., decode_fec=1)`.

---

## 5. RTCP Control & Recovery Protocol

### 5.1 Canonical Name (CNAME) Stream Association
Both the video track (SSRC 1) and audio track (SSRC 2) are transmitted with the identical RTCP Canonical Name:
```
CNAME: mist-stream
Stream ID: mist-av-stream
```
This enables the client media engine to correlate the independent 90 kHz video clock and 48 kHz audio clock to a common NTP reference time.

### 5.2 Picture Loss Indication (PLI, RFC 4585)
- **Format:** Payload-specific feedback message, FMT = 1 (`PLI`), Payload Type = 206 (`PSFB`).
- **Trigger:** Dispatched by `VideoTrackReceiver` when downstream H.264 parser encounters corrupted NAL headers, packet sequence discontinuity, or decoder error.
- **Host Action:** `VideoTrackSender` catches the PLI and flags `forceKeyframeNext_ = true` on the NVENC encoder, delivering a complete IDR keyframe within 1 RTT without stream buffering.

---

## 6. SCTP Application Data Channels

| Channel Label | Reliability | Ordering | Stream ID | Purpose |
|---|---|---|---|---|
| `session-control` | Reliable | Ordered | 0 | Client connection requests, pause, resume, bitrate caps. |
| `diagnostics` | Unreliable | Unordered | 1 | Real-time round-trip latency, frame loss stats, host telemetry. |
