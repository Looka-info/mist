# ADR 0010: Native WebRTC Transport Library Selection

## Status
Accepted

## Context
Milestone 3 requires establishing low-latency, encrypted, peer-to-peer audio/video streaming and control transport between the native Mist host and client applications. We evaluated potential WebRTC libraries for native Windows C++20 integration.

### Evaluated Options:
1. **Google WebRTC (libwebrtc / Chromium tree):**
   - *Pros:* Complete reference WebRTC implementation, full browser parity.
   - *Cons:* Massive repository footprint (tens of gigabytes), custom `depot_tools` and `gn`/`ninja` build system incompatible with pure CMake, monolithic thread models, heavy memory overhead, difficult to maintain in a minimal C++ project.
2. **Raw UDP / Custom SRTP / Pion C-Go wrapper:**
   - *Pros:* High control over custom packets.
   - *Cons:* Reinventing standard ICE, DTLS-SRTP key exchange, and STUN/TURN handling violates security and milestone rules. Go runtime wrapper introduces foreign GC runtime overhead and memory complexity.
3. **libdatachannel (paullouisageneau/libdatachannel):**
   - *Pros:* Lightweight, native modern C++ (C++17/C++20), pure CMake build system, MPL-2.0 license, native Windows x64 support, built-in MbedTLS cryptographic provider (no OpenSSL installation required), direct H.264 RTP packetizer/depacketizer with FU-A fragmentation, integrated WebSocket client/server support, active open-source maintenance.
   - *Cons:* Does not include built-in video codecs or complex software audio processing (e.g. echo cancellation), which is ideal for Mist since Mist uses custom hardware NVENC encoding and Media Foundation D3D11 decoding.

## Decision
We select **libdatachannel** (pinned to stable release `v0.22.4`) as the initial native WebRTC transport backend for Mist.

## Consequences
- Clean CMake `FetchContent` integration with reproducible dependencies.
- Zero external OpenSSL runtime DLL deployment requirements by utilizing MbedTLS.
- Strict isolation behind `mist::transport::ITransport` ensures the engine core remains completely decoupled from `libdatachannel` data types.
