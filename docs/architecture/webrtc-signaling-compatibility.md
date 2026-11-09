# Mist WebRTC Signaling & Input Protocol Compatibility Specification

**Document Version:** 1.0.0  
**Target Milestone:** Milestone 28 (Release Qualification)  
**Applicable Subsystems:** `services/signaling`, `web/js/mist-web-client.js`, `engine/input`, `engine/decode`

---

## 1. Overview & Architecture

Mist utilizes a hybrid architecture for game streaming:
- **Signaling Server:** Rust-based asynchronous WebSocket server (`services/signaling`) utilizing `tokio-tungstenite` and `serde_json` with snake_case message framing.
- **Web Guest Client:** Pure vanilla TypeScript/JavaScript browser client (`web/js/mist-web-client.js`) using standard W3C `RTCPeerConnection`, `RTCDataChannel`, and optional WebCodecs hardware decode.
- **Native Host & Desktop Guest:** C++20 media engine (`engine/input`, `engine/decode`, `engine/webrtc`) paired with Rust application orchestration (`mist-core`, `mist-pairing`).

All signaling communication occurs over WebSocket text frames encoded in JSON. All user input telemetry flows over an SCTP DataChannel labelled `"mist-input"` using packed little-endian binary buffers matching `mist::input::InputPacketHeader`.

```
+------------------+          WebSocket (JSON)         +----------------------+
|  Mist Web Guest  | <==============================> |  Signaling Server    |
| (Browser/Mobile) |                                  |  (services/signaling)|
+------------------+                                  +----------------------+
         |                                                       ^
         | SCTP DataChannel ("mist-input")                       | WebSocket
         | 48-byte InputPacketHeader + Payloads                  |
         v                                                       v
+--------------------------------------------------------------------+
|                         Mist Host Daemon                           |
|      (DirectX 11 / NVENC / WebRTC / Input Injection Engine)        |
+--------------------------------------------------------------------+
```

---

## 2. Signaling Wire Protocol Specification

The signaling server multiplexes session rooms identified by `session_id` and authorized via a bearer `token`. All JSON payloads use `{ "type": "<action>", ... }` discriminant tagging.

### 2.1 Message Definitions

#### 1. `register` (Client -> Server)
Registers a connection within a session room.
```json
{
  "type": "register",
  "session_id": "string",
  "role": "host" | "guest",
  "token": "string",
  "guest_id": "string (optional for host, required for guest)"
}
```

#### 2. `registered` (Server -> Client)
Acknowledges registration and reports whether a counterpart peer is already in the room.
```json
{
  "type": "registered",
  "session_id": "string",
  "role": "host" | "guest",
  "peer_connected": true,
  "guest_id": "string"
}
```

#### 3. `peer_joined` (Server -> Broadcast)
Notifies existing room participants that a new peer joined.
```json
{
  "type": "peer_joined",
  "session_id": "string",
  "peer_role": "host" | "guest",
  "guest_id": "string"
}
```

#### 4. `offer` (Host -> Server -> Guest)
Transmits SDP offer for peer negotiation.
```json
{
  "type": "offer",
  "session_id": "string",
  "sdp": "v=0\r\no=...",
  "target_guest_id": "string"
}
```

#### 5. `answer` (Guest -> Server -> Host)
Transmits SDP answer completing negotiation.
```json
{
  "type": "answer",
  "session_id": "string",
  "sdp": "v=0\r\no=...",
  "guest_id": "string"
}
```

#### 6. `candidate` (Bidirectional)
Exchanges trickle-ICE candidates.
```json
{
  "type": "candidate",
  "session_id": "string",
  "candidate": "candidate:...",
  "mid": "0",
  "guest_id": "string"
}
```

#### 7. `bye` (Bidirectional)
Graceful teardown notification.
```json
{
  "type": "bye",
  "session_id": "string",
  "guest_id": "string"
}
```

#### 8. `error` (Server -> Client)
Authentication failure, room expiry, or rate limit throttling error.
```json
{
  "type": "error",
  "message": "string"
}
```

---

## 3. WebRTC Negotiation & ICE Lifecycle Guard

### 3.1 Race Condition Resolution: Early Candidate Queue
In real-world networks, trickle ICE candidates often arrive via WebSocket before `RTCPeerConnection.setRemoteDescription(offer)` completes asynchronously. Attempting `addIceCandidate()` prior to setting the remote description throws `InvalidStateError`.

**Mitigation:**
1. The guest client maintains an in-memory `iceCandidateQueue = []`.
2. Upon receiving message type `candidate`:
   - If `pc.remoteDescription` is valid: `await pc.addIceCandidate(...)` immediately.
   - If `pc.remoteDescription` is null: enqueue candidate into `iceCandidateQueue`.
3. In `handleSignalingMessage('offer')`:
   - Perform `await pc.setRemoteDescription(...)`.
   - Drain and submit all queued candidates via `await pc.addIceCandidate(...)`.
   - Clear `iceCandidateQueue`.

---

## 4. SCTP Binary Input Protocol Specification

Input packets sent across the `"mist-input"` SCTP DataChannel must match the native C++ engine packing layout specified in `mist::input::InputPacketHeader` (`sizeof == 48`) followed by the respective event payload.

All numeric fields are **little-endian**.

### 4.1 Packet Header Layout (`InputPacketHeader`) — 48 Bytes

| Offset (Bytes) | Field | Type | Description |
|:---|:---|:---|:---|
| 0 .. 1 | `version` | `uint16_t` | Input protocol version (always `1`) |
| 2 | `inputType` | `uint8_t` | `1`: Key, `2`: MouseMove, `3`: MouseButton, `4`: MouseWheel, `5`: Gamepad |
| 3 | `flags` | `uint8_t` | Bitfield: Bit 0=Reliable, Bit 1=Snapshot, Bit 2=Emergency |
| 4 .. 7 | `sequenceNumber` | `uint32_t` | Monotonically increasing sequence number per session |
| 8 .. 15 | `timestampNs` | `uint64_t` | Nanoseconds timestamp (`performance.now() * 1e6`) |
| 16 .. 47 | `sessionId` | `char[32]` | ASCII/UTF-8 session string, null-padded |

### 4.2 Payload Layouts

#### `MouseMovePayload` (InputType = 2) — 20 Bytes (Total 68 Bytes)
| Offset | Field | Type | Description |
|:---|:---|:---|:---|
| 0 .. 3 | `deltaX` | `int32_t` | Pointer lock relative X delta |
| 4 .. 7 | `deltaY` | `int32_t` | Pointer lock relative Y delta |
| 8 | `isAbsolute` | `uint8_t` | `0` = Relative (FPS gaming), `1` = Absolute |
| 9 .. 11 | `reserved` | `uint8_t[3]` | Padding (zeros) |
| 12 .. 15 | `normalizedX` | `float` (IEEE 754) | Normalized [0.0, 1.0] viewport X |
| 16 .. 19 | `normalizedY` | `float` (IEEE 754) | Normalized [0.0, 1.0] viewport Y |

#### `MouseButtonPayload` (InputType = 3) — 4 Bytes (Total 52 Bytes)
| Offset | Field | Type | Description |
|:---|:---|:---|:---|
| 0 | `button` | `uint8_t` | `1`=Left, `2`=Right, `3`=Middle, `4`=X1, `5`=X2 |
| 1 | `isDown` | `uint8_t` | `1`=Pressed, `0`=Released |
| 2 .. 3 | `reserved` | `uint16_t` | Padding (zeros) |

#### `KeyPayload` (InputType = 1) — 12 Bytes (Total 60 Bytes)
| Offset | Field | Type | Description |
|:---|:---|:---|:---|
| 0 .. 3 | `virtualKey` | `uint32_t` | Virtual key code (e.g. VK_A = 0x41) |
| 4 .. 7 | `scanCode` | `uint32_t` | Hardware scan code (or 0) |
| 8 | `action` | `uint8_t` | `0`=Press, `1`=Release (matches `KeyAction`) |
| 9 | `isExtended` | `uint8_t` | `0` or `1` |
| 10 .. 11 | `reserved` | `uint16_t` | Padding (zeros) |

#### `MouseWheelPayload` (InputType = 4) — 8 Bytes (Total 56 Bytes)
| Offset | Field | Type | Description |
|:---|:---|:---|:---|
| 0 .. 3 | `wheelDeltaX` | `int32_t` | Horizontal scroll delta |
| 4 .. 7 | `wheelDeltaY` | `int32_t` | Vertical scroll delta |

---

## 5. Security & Verification

1. **Origin & Token Verification:** Signaling server checks bearer token before peer binding. Mismatched tokens emit `{"type":"error","message":"Invalid session authorization token"}`.
2. **Buffer Bounds:** Deserializers on both host and guest strictly reject packets smaller than 48 bytes.
3. **Zero Synthetic Metric Generation:** Telemetry relies entirely on `RTCPeerConnection.getStats()` rather than client-side randomized timers.
