# Mist Security Architecture & Threat Model

## 1. Security Overview

Mist handles desktop capture, input injection, and low-latency network streaming. Due to the high-privilege nature of desktop screen access and input simulation on Windows, security and isolation are first-class engineering requirements.

---

## 2. Threat Model & Attack Surfaces

### 2.1 Untrusted Network Inputs
- **Signaling Server Compromise:** A malicious or compromised signaling relay attempting to hijack or inject peer connection metadata.
  - *Mitigation:* End-to-end cryptographic mutual authentication and session key agreement (e.g. DTLS-SRTP / Noise protocol). The signaling server never receives raw media keys or user credentials.
- **Malformed Network Packets:** Exploitation of video demuxers, audio decoders, or input packet parsers via buffer overflows or memory corruption.
  - *Mitigation:* Strict C++20 bounds checking (`std::span`), zero unvalidated pointer arithmetic, explicit packet length validation before decoding.
- **Timing Side-Channel Attacks:** Attempting to infer authentication tokens or session secret hashes by measuring comparison response times.
  - *Mitigation:* Implemented `mist::security::constantTimeEquals`, ensuring comparison time depends only on string length and not matching character prefixes.

### 2.2 Host System Privilege Boundaries
- **Remote Input Injection Abuse:** Preventing unauthorized remote processes from injecting synthetic keystrokes or mouse clicks to bypass Windows UAC (User Account Control).
  - *Mitigation:* Host input injection service requires pairing authorization. In future milestones, administrative elevation for UAC interaction must use isolated Windows service boundaries with explicit user consent.
- **Credential Storage:** Storing access tokens or shared secrets in plain-text configuration files or logs.
  - *Mitigation:* Strict prohibition of credentials in `mist.ini` / `AppConfig`. `mist::common::Logger` implements automated token redaction (`[REDACTED_SENSITIVE_DATA]`). Secrets are stored using Windows DPAPI (`CryptProtectData`) or the Windows Credential Locker in production milestones.

### 2.3 Video Bitstream & Decoder Attack Surfaces (Milestone 2)
- **Malformed NAL Units & Truncated Packets:** Malicious or corrupted video bitstream payloads designed to trigger out-of-bounds reads or buffer overflows.
  - *Mitigation:* `H264Parser` uses strict pointer-range arithmetic with explicit end-of-buffer bounds checks. Start code scanning validates 3-byte and 4-byte boundaries before byte dereferencing. Emulation prevention stripping rejects corrupted sequences without allocating unbounded memory.
- **Exp-Golomb Integer Overflow / DoS:** Corrupted SPS syntax elements triggering integer overflows in macroblock dimension calculations.
  - *Mitigation:* Exp-Golomb reader explicitly validates zero-bit run length (capped at 31 bits) and rejects shifts exceeding 32-bit integer capacity. Parsed dimensions are validated against hardware limits (max 4096x4096).
- **VRAM / GPU Memory Exhaustion:** Flooding the decoder with high-frequency frames to exhaust GPU VRAM or DXGI surfaces.
  - *Mitigation:* Decoded frame queue is strictly bounded to a maximum of 2 pending frames. When downstream presentation lags, the oldest frame is dropped immediately, guaranteeing that unpresented textures are returned to the hardware decoder's allocator.
- **Host / Client VRAM Isolation:** Direct3D 11 device contexts are private to each client process. Zero-copy hardware decoded DXGI surfaces are managed via `IMFDXGIDeviceManager` within the local GPU memory boundary without arbitrary memory exposure.

### 2.4 WebRTC Transport & Signaling Attack Surfaces (Milestone 3)
- **Man-in-the-Middle (MitM) & Media Eavesdropping:** Unauthorized packet inspection or tampering on public network paths.
  - *Mitigation:* WebRTC mandates DTLS 1.2+ for key exchange and SRTP (AES-GCM / AES-CTR) for media encryption. Ephemeral ECDSA (P-256) certificates are generated per session. Fingerprints are exchanged out-of-band via signaling and verified prior to DTLS handshake completion.
- **Signaling Hijacking & Unauthorized Registration:** Malicious clients registering to unassigned or arbitrary session IDs.
  - *Mitigation:* `mist-signaling` verifies non-empty bearer tokens on all registration requests. Sessions are strictly scoped to pairs of exactly one `host` and one `client`. Role collisions or unauthorized peer registrations are immediately rejected with WebSocket close code 4003 (`Unauthorized`).
- **Memory Bomb / RTP Packet Fragmentation Flooding:** Flooding the depacketizer with unbounded FU-A fragments.
  - *Mitigation:* `VideoTrackReceiver` enforces maximum buffer size limits and drops corrupted or incomplete NALUs upon sequence number discontinuity.
- **DLL Preloading / Hijacking Prevention:** Third-party cryptographic libraries dynamically loading vulnerable system DLLs.
  - *Mitigation:* `MbedTLS v3.6.2` is compiled statically directly into `mist_transport` with `MBEDTLS_SSL_DTLS_SRTP`. No dynamic OpenSSL DLLs are loaded at runtime.

### 2.5 Audio Subsystem & Privacy Boundaries (Milestone 4)
- **Host Microphone Eavesdropping & Surveillance Risk:** Remote streaming applications capturing unintended ambient room audio via host microphones.
  - *Mitigation:* `WasapiAudioCapture` is strictly constrained to render endpoints (`eRender`, `eMultimedia`) in `AUDCLNT_STREAMFLAGS_LOOPBACK` mode. Audio input endpoints (`eCapture` / microphones) are completely excluded from the capture interface. Audio capture is strictly opt-in via explicit `--audio` configuration.
- **Malformed Opus Payloads & Memory Corruption:** Malicious peers transmitting malformed Opus bitstreams designed to exploit codec buffer overflows.
  - *Mitigation:* Employs official reference `xiph/opus` v1.5.2 with strict packet bounds checking. The decoder validates maximum frame sizes (120 ms / 5760 samples per frame) before decoding. Memory allocations are static and bounded.
- **Playout Buffer Bloat / Memory Exhaustion:** Flooding client with audio packets to exhaust client RAM.
  - *Mitigation:* `WasapiAudioPlayback` bounds its jitter queue to a hard maximum of 80 ms (~8 packets / < 100 KB RAM). Excess packets are dropped unconditionally.
- **Resampler Numerical Stability & NaN Injection:** Corrupted audio samples destabilizing floating-point math.
  - *Mitigation:* `AudioResampler` validates input sample rates against sanity bounds (8,000 Hz to 192,000 Hz) and channel counts (1 to 8). Downmixing clamps floating-point values to `[-1.0, 1.0]`, preventing clipping and NaN propagation.

### 2.6 Remote Input Attack Surfaces & Host Sovereignty (Milestone 5)
- **Unauthorized Remote Control & Session Hijacking:** Unauthenticated remote peers attempting to inject synthetic keyboard or mouse commands.
  - *Mitigation:* Remote input is **disabled by default** (`InputPermissionState::Disabled`). Control requires affirmative host authorization via `InputPermissionManager`. Packets must match the active session ID; cross-session inputs are immediately dropped and logged.
- **Physical Host Sovereignty & Local Override:** Malicious or rogue clients attempting to lock out the local host user.
  - *Mitigation:* The local physical user maintains absolute sovereignty. Any local host physical input activity takes precedence, and local CLI/GUI controls can revoke remote permissions instantly (`revokePermissions()`).
- **Input Flooding & Denial-of-Service (DoS):** Compromised clients flooding high-frequency input packets to overwhelm the host OS event loop.
  - *Mitigation:* Packets are regulated by a sliding-window rate limiter enforcing a maximum of 2,000 events/second per session. Ingress queues are bounded to 64 items with oldest-item dropping and relative delta coalescing.
- **Arithmetic Anomalies & Malformed Payloads:** Exploiting deserialization logic via out-of-range coordinates or invalid enums.
  - *Mitigation:* Strict binary packet size verification (`48 <= size <= 256`). Floating-point coordinates are sanitized with `std::isnan()` and `std::isinf()` and clamped to `[0.0, 1.0]`. Enum discriminators undergo strict range checks.
- **Windows User Interface Privilege Isolation (UIPI):** Remote synthetic input interacting with elevated UAC administrator prompts.
  - *Mitigation:* Mist strictly adheres to Windows UIPI security boundaries. Standard user-mode host processes cannot inject input into higher-integrity processes. Mist does not attempt fragile, dangerous UIPI bypasses or DLL injection.
- **Kernel-Mode Driver Safety:** Third-party driver installation vulnerabilities.
  - *Mitigation:* Mist **never** executes silent or unattended kernel driver installations. Virtual controller support uses pluggable dynamic probing (`HostVirtualGamepad`) and falls back gracefully when ViGEmBus is absent.
- **Exclusion of Remote Execution Capabilities:**
  - *Mitigation:* The input protocol strictly encapsulates discrete hardware scan codes, mouse deltas, and controller states. It contains **zero** shell execution, process spawning, file transfer, or clipboard sharing capabilities.

### 2.7 Device Pairing Security & Granular Authorization (Milestone 6)
- **Brute-Force Pairing Code Enumeration:** Attackers guessing pairing codes on local or remote networks.
  - *Mitigation:* Codes are 6 characters drawn from a 32-symbol alphanumeric alphabet (`32^6 = 1,073,741,824` combinations) excluding ambiguous characters (`0`, `O`, `1`, `I`). Codes expire after 300 seconds (single-use). 5 consecutive failed attempts trigger an immediate 300-second lockout.
- **Timing Side Channels in Pairing Verification:**
  - *Mitigation:* Verification uses `mist::security::constantTimeEquals` across all comparison paths.
- **Overprivileged Remote Client Capabilities:** Clients granted stream viewing obtaining unintended access to host input or audio.
  - *Mitigation:* Granular capability permissions (`allowVideo`, `allowAudio`, `allowKeyboard`, `allowMouse`, `allowGamepad`) are explicitly configured and stored per paired device. Viewing video does not grant input or audio capture access.
- **Trusted Devices Store Tampering & Crash Corruption:**
  - *Mitigation:* `PairingManager` uses atomic file replacement (`trusted_devices.json.tmp` renamed to `trusted_devices.json`) to prevent partial writes during crashes or unexpected termination.

---

## 3. Cryptographic & Device Authorization Requirements

1. **Device Identity (`mist::security::DeviceIdentity`):**
   - Each host and client device possesses a unique cryptographic identity and public key fingerprint.
   - Device IDs are verified during initial pairing.
2. **Session Pairing Tokens (`mist::security::AuthToken`):**
   - Pairing tokens have strict expiration lifetimes (`expiresAtEpochSec`).
   - Expired tokens are immediately rejected by `AuthToken::isExpired()`.
3. **Session Credentials (`mist::security::SessionCredentials`):**
   - High-entropy cryptographic shared secrets (minimum 32 hex characters / 128-bit security).
   - Validated prior to session initiation.

---

## 4. Windows Platform Isolation

1. **Platform API Boundary:**
   - All Windows Win32 API calls (`GetLastError`, `SetProcessDpiAwarenessContext`, `QueryPerformanceCounter`) are encapsulated inside `platform/windows`.
   - Engine subsystems (`engine/*`) remain pure, portable C++20 without direct includes of `<windows.h>`.
2. **Error Translation:**
   - System error codes are cleanly translated into typed `mist::common::Error` structs with clear subsystem ownership, eliminating opaque numeric return codes.

---

## 5. Code Signing & Release Integrity Verification (Milestone 6)

1. **Authenticode Signatures (`scripts/sign-release.ps1`):**
   - Distributed Windows binaries (`mist_host.exe`, `mist_client.exe`, `mist_desktop.exe`, `mist-signaling.exe`) are signed with Windows Authenticode using Microsoft `signtool.exe`.
   - Incorporates trusted timestamping (`http://timestamp.digicert.com`) to preserve signature validity after certificate expiration.
   - For local development and testing, automatically creates and signs with an isolated development code-signing certificate (`CN=Mist Development Code Signing`).
2. **Release Manifest (`release-manifest.json`):**
   - Every distribution archive includes a cryptographically verifiable release manifest recording SHA-256 hashes, file sizes, version metadata, and build timestamps.
   - Installers and update verifiers check file integrity against the manifest before execution.
