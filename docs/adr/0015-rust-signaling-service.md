# ADR 0015: Rust Tokio/Tungstenite Signaling Microservice Architecture

## Status
Accepted

## Context
WebRTC PeerConnection establishment requires an out-of-band signaling channel to exchange SDP session descriptions (offers/answers) and ICE candidate addresses between the streaming host and client. While signaling is not on the high-bandwidth media path, it must be robust, asynchronous, low-latency, and memory-safe under concurrent session connections.

## Decision
1. **Lightweight Standalone Binary:**
   - Implement the signaling server as a standalone native binary (`mist-signaling.exe`) written in modern Rust using `tokio` (asynchronous runtime) and `tokio-tungstenite` (WebSocket transport).
   - This keeps the C++ media engine completely unburdened by HTTP/WebSocket server orchestration and enables independent deployment/scaling.
2. **Room-Based In-Memory Dispatch:**
   - Use an asynchronous, thread-safe session map (`Arc<RwLock<HashMap<String, Session>>>`) pairing exactly two peers: Host and Client.
   - Forward structured JSON signaling payloads (`offer`, `answer`, `ice-candidate`, `bye`) between peers in the same session identifier without inspecting or retaining media payloads.
3. **Session Lifecycle & Tear-down:**
   - Automatically broadcast peer disconnect notifications when a WebSocket connection drops.
   - Clean up abandoned rooms immediately when the last connected participant disconnects.
4. **Native C++ WebSocket Client:**
   - The C++ host and client applications connect to the signaling microservice using an asynchronous WebSocket client implemented on top of `libdatachannel`'s internal WebSocket transport, preserving unified event loop integration.

## Consequences
- Zero external runtime dependencies (no Node.js, Python, or external web servers required).
- Minimal memory footprint (< 15 MB working set under concurrent session loads).
- Clean boundary between out-of-band session establishment and peer-to-peer real-time media streaming.
