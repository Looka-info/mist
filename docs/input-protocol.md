# Mist Remote Input Protocol Specification (Version 1.0)

## 1. Overview

The Mist Remote Input Protocol defines the binary wire format and transmission rules for streaming low-latency user input (keyboard, mouse, gamepad, and permission controls) from a Mist client to a Mist host over WebRTC SCTP DataChannels.

### Design Principles
- **Minimal Framing Overhead:** Compact binary encoding with fixed-size packet headers (48 bytes).
- **Zero Memory Allocation:** Fixed upper bounds (`<= 256` bytes) allow decoding directly from network stack buffers without heap allocations.
- **Differentiated Quality of Service:** Strict channel segregation between state-sensitive discrete events and latency-sensitive continuous motion.
- **Defensive Parsing:** Validation against `NaN`, infinite floats, out-of-range enums, buffer overruns, and packet replay.

---

## 2. Transport Channel Mapping

Input messages are multiplexed across two dedicated WebRTC SCTP DataChannels:

| DataChannel Label | SCTP Reliability Configuration | Message Types Carried | Guarantee |
|---|---|---|---|
| `input-events` | Reliable, Ordered | `KeyEvent`<br>`MouseButtonEvent`<br>`MouseWheelEvent`<br>`ResetState`<br>`PermissionMessage` | Lossless, strictly ordered delivery. Eliminates stuck keys or missed button clicks. |
| `input-state` | Unreliable, Unordered (`maxPacketLifeTime = 50ms`) | `MouseMoveEvent`<br>`GamepadState` | Minimum latency, loss-tolerant delivery. Stale motion updates are discarded by the kernel SCTP stack. |

---

## 3. Wire Format Specification

All multi-byte numeric fields are encoded in **Little-Endian** byte order.

### 3.1 Packet Header Layout (`InputPacketHeader`)

Every input datagram begins with an identical 48-byte header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Magic: 0x5453494D ("MIST")                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Version: 0x0100         |          Reserved0            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Sequence Number (Bits 0-31)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Sequence Number (Bits 32-63)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             Timestamp Microseconds (Bits 0-31)                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             Timestamp Microseconds (Bits 32-63)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Session ID                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Payload Type         |          Payload Size         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                       Reserved1 (16 bytes)                    +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

#### Header Fields:
- **Magic (32 bits):** Value `0x5453494D` (ASCII `"MIST"`). Packets with differing magic are rejected immediately.
- **Version (16 bits):** Protocol major/minor version. Current version is `0x0100` (1.0).
- **Reserved0 (16 bits):** Reserved for header alignment; must be set to `0`.
- **Sequence Number (64 bits):** Monotonically increasing sequence number per sender session. Used to detect reordering and duplicate packets.
- **Timestamp (64 bits):** Unix epoch timestamp in microseconds recorded at input capture.
- **Session ID (32 bits):** Active 32-bit session token identifier assigned during signaling handshake.
- **Payload Type (16 bits):** Enumerated identifier of the enclosed payload (see Section 3.2).
- **Payload Size (16 bits):** Size in bytes of the payload directly following the 48-byte header.
- **Reserved1 (16 bytes):** 128-bit reserved alignment block; zero-initialized.

---

### 3.2 Payload Type Identifiers

| Identifier | Value | Struct / Event | Standard Channel |
|---|---|---|---|
| `Unknown` | `0` | None | None |
| `KeyboardKey` | `1` | `KeyEvent` | `input-events` |
| `MouseMove` | `2` | `MouseMoveEvent` | `input-state` |
| `MouseButton` | `3` | `MouseButtonEvent` | `input-events` |
| `MouseWheel` | `4` | `MouseWheelEvent` | `input-events` |
| `GamepadState` | `5` | `GamepadState` | `input-state` |
| `ResetState` | `6` | Empty (Header only) | `input-events` |
| `Permission` | `7` | `PermissionMessage` | `input-events` |

---

### 3.3 Payload Struct Definitions

#### 3.3.1 `KeyEvent` (Payload Type 1, 16 bytes)
```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Scan Code           |          Virtual Key          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Action (0/1) |  Extended (0/1)|         Modifier Mask        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Reserved                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
- **Scan Code (`uint16_t`):** Hardware scan code (e.g. `0x11` for WASD 'W').
- **Virtual Key (`uint16_t`):** Windows Virtual Key Code (`VK_...`).
- **Action (`uint8_t`):** `0 = KeyDown`, `1 = KeyUp`.
- **Extended (`uint8_t`):** `1` if extended key (Right Alt, Right Ctrl, Arrow keys, Numpad Enter); `0` otherwise.
- **Modifier Mask (`uint16_t`):** Bitmask of active modifier keys (`Shift=0x01`, `Ctrl=0x02`, `Alt=0x04`, `Meta=0x08`).

#### 3.3.2 `MouseMoveEvent` (Payload Type 2, 24 bytes)
```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Delta X                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Delta Y                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Normalized X (IEEE 754 float)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Normalized Y (IEEE 754 float)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Absolute(0/1) |                   Reserved                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
- **Delta X / Y (`int32_t`):** Relative pixel offset since last sample. Used for FPS/mouselook cameras.
- **Normalized X / Y (`float`):** Absolute coordinates mapped to `[0.0, 1.0]`. Clamped and checked for `NaN`.
- **Absolute (`uint8_t`):** `1` = Absolute positioning mode; `0` = Relative delta mode.

#### 3.3.3 `MouseButtonEvent` (Payload Type 3, 16 bytes)
```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Button Code  |  Action (0/1) |           Reserved0           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Normalized X (IEEE 754 float)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   Normalized Y (IEEE 754 float)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
- **Button Code (`uint8_t`):** `0 = Left`, `1 = Right`, `2 = Middle`, `3 = X1`, `4 = X2`.
- **Action (`uint8_t`):** `0 = ButtonDown`, `1 = ButtonUp`.
- **Normalized X / Y (`float`):** Cursor location at the time of click.

#### 3.3.4 `MouseWheelEvent` (Payload Type 4, 16 bytes)
```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Delta X                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Delta Y                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Reserved                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
- **Delta X (`int32_t`):** Horizontal scroll delta (positive = right, negative = left).
- **Delta Y (`int32_t`):** Vertical scroll delta (positive = forward/up, negative = backward/down; standard notch = 120).

#### 3.3.5 `GamepadState` (Payload Type 5, 20 bytes)
```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| User Index(0-3)| Connected(0/1)|         Buttons Mask          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Left Trigger | Right Trigger |           Reserved0           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Thumb LX            |           Thumb LY            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Thumb RX            |           Thumb RY            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Reserved1                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
- **User Index (`uint8_t`):** Controller slot (`0` to `3`).
- **Connected (`uint8_t`):** `1` if controller is plugged in; `0` if disconnected.
- **Buttons Mask (`uint16_t`):** Standard XInput digital button bitmask (DPAD, Start, Back, Thumbs, Bumpers, A/B/X/Y).
- **Left / Right Trigger (`uint8_t`):** Analog trigger value `0` to `255` (threshold filtered).
- **Thumb LX / LY / RX / RY (`int16_t`):** Signed 16-bit analog stick values (`-32768` to `+32767`, deadzone filtered).

#### 3.3.6 `PermissionMessage` (Payload Type 7, 16 bytes)
```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Action (0-3)  | Feature Mask  |           Reserved            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Target Session                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Reserved1                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```
- **Action (`uint8_t`):** `0 = Request`, `1 = Grant`, `2 = Deny`, `3 = Revoke`.
- **Feature Mask (`uint8_t`):** Bitmask of requested/granted features (`Keyboard=0x01`, `Mouse=0x02`, `Gamepad=0x04`).
- **Target Session (`uint32_t`):** Session ID for which permission is being negotiated.

---

## 4. State Synchronization & Recovery Rules

1. **Client Focus Loss:**
   - When the client video window loses focus (`WM_KILLFOCUS` or deactivated), the client immediately sends a `ResetState` packet over `input-events`.
   - The host executes an emergency release of all currently tracked keys and mouse buttons.
2. **Session Disconnection or Revocation:**
   - Upon connection closure or host permission revocation, the host unconditionally executes `releaseAllKeys()` and `releaseAllButtons()`.
3. **Queue Overflow Coalescing:**
   - The host input dispatcher maintains a maximum queue depth of 64 events.
   - If relative mouse moves accumulate faster than the dispatch thread consumes them, consecutive displacement deltas are coalesced into a single event:
     $$\Delta x_{\text{total}} = \sum \Delta x_i, \quad \Delta y_{\text{total}} = \sum \Delta y_i$$
   - Gamepad snapshots in queue are replaced with the most recent snapshot.

---

## 5. Security Validation

- **Length Verification:** Packets with `size < 48` or `size > 256` bytes are dropped.
- **Session Matching:** Packets with `session_id != active_session_id` are rejected.
- **Float Sanitization:** Any coordinate where `std::isnan(x) || std::isnan(y)` is rejected.
- **Rate Limiting:** Ingress is capped at 2,000 events/second per session via a sliding token window.
