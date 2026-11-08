# ADR 0012: Session Signaling Architecture and Protocol

## Status
Accepted

## Context
WebRTC peers require an out-of-band signaling mechanism to discover one another, exchange session descriptions (SDP Offer / Answer), and coordinate network connectivity candidates (ICE candidates). Signaling must be lightweight, secure, and completely decoupled from media transport.

### Evaluated Approaches:
1. **Direct HTTP Polling / Long-Polling:** High latency, polling overhead, poor real-time candidate exchange.
2. **Third-Party Managed Cloud Signaling:** Violates local/sovereign deployment goals, recurring infrastructure costs, and out of scope for low-budget self-hosted streaming.
3. **Dedicated Asynchronous Rust WebSocket Service:** High performance, memory safety, minimal resource consumption, native JSON serialization, and cross-platform concurrency.

## Decision
1. Implement a standalone Rust signaling microservice under `services/signaling` using `tokio`, `tokio-tungstenite`, and `serde_json`.
2. Implement WebSocket-based bidirectional JSON messaging:
   - `Register`: Host or client registers with session ID and cryptographic auth token.
   - `Offer`: Host transmits SDP offer payload to client.
   - `Answer`: Client transmits SDP answer payload to host.
   - `Candidate`: Either peer transmits trickle ICE candidate information.
   - `Heartbeat`: Periodic ping/pong detecting dead connections.
   - `Disconnect`: Graceful notification of session teardown.
3. In C++, implement `ISignalingClient` using `rtc::WebSocket` from `libdatachannel`, providing native non-blocking asynchronous event dispatch.
4. Support local offline loopback testing mode where offer and answer are piped directly in-memory without requiring an active external network socket.

## Consequences
- High-throughput, memory-safe signaling server capable of handling hundreds of concurrent connection setups.
- Media bytes never pass through the signaling service, keeping server CPU and bandwidth utilization near zero.
