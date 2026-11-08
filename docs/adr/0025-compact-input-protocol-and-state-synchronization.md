# ADR 0025: Compact Input Protocol and State Synchronization

## Status
Accepted

## Context
High-performance game streaming requires transmitting input events with minimal wire overhead, zero JSON/text parsing latency, and robust resistance to packet loss, reordering, and bufferbloat. Input protocols must also defend against malformed payloads, out-of-range numerical coordinates, and desynchronization between client and host state.

## Decision
1. **Compact Fixed Binary Protocol (Version 1):**
   - Packets use a fixed 48-byte header (`InputPacketHeader`) aligned with little-endian network serialization:
     - Magic identifier: `0x5453494D` (`"MIST"`).
     - Protocol Version: `0x0100` (Version 1.0).
     - Monotonic Packet Sequence Number (`uint64_t`).
     - Microsecond Host/Client Timestamp (`uint64_t`).
     - Session ID (`uint32_t`).
     - Payload Type (`uint16_t`) and Payload Size (`uint16_t`).
     - Reserved alignment fields (16 bytes).
   - Strict size bounds: Any packet with total size `< 48` bytes or `> 256` bytes is rejected immediately prior to memory allocation.
2. **Defensive Deserialization & Sanitization:**
   - Float coordinates for normalized absolute mouse positioning (`[0.0, 1.0]`) are sanitized against `NaN` and `Inf` using `std::isnan` and clamped to `[0.0, 1.0]`.
   - Discrete enum types (`InputType`, `MouseButton`, `KeyAction`, `PermissionAction`) undergo strict range validation.
   - Any serialization error returns `Result<InputEvent>::failure()` without throwing exceptions.
3. **Bounded Dispatch Queue & Delta Coalescing:**
   - The host input dispatcher maintains a thread-safe bounded queue limited to 64 items.
   - If the queue exceeds capacity, the oldest unhandled continuous state packet is dropped to prioritize the freshest user intent.
   - Multiple consecutive relative mouse move deltas pending in the queue are coalesced into a single composite displacement `(dx1 + dx2, dy1 + dy2)`, drastically reducing system call overhead without sacrificing precision.
   - Gamepad state snapshots in the queue replace prior un-dispatched gamepad snapshots.
4. **Client Focus Loss Synchronization:**
   - The client application monitors OS window activation (`WM_ACTIVATE`, `WM_KILLFOCUS`).
   - On focus loss, the client dispatches a synthetic `ResetState` event over the reliable `input-events` DataChannel to ensure no keys remain held down when the user switches away from the stream viewer.

## Consequences
- Fixed memory footprint and sub-millisecond serialization/deserialization.
- Complete protection against memory corruption, buffer overflows, and arithmetic anomalies.
- Automatic queue shedding during system hitches prevents lag accumulation.
- Clean recovery from client alt-tabbing and focus transitions.
