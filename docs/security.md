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
