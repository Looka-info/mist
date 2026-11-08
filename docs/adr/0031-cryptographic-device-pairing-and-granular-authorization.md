# ADR 0031: Cryptographic Device Pairing and Granular Authorization

## Status
Accepted

## Context
Pairing a remote client to a host PC must be straightforward for gamers while maintaining cryptographic security, replay protection, and granular permissions. Connecting a display must never silently grant full remote keyboard, mouse, or controller control.

## Decision
1. **Short-Lived 6-Character Pairing Codes:**
   - Host generates single-use alphanumeric pairing codes (e.g. `MIST-829` or `5F1-2U8`) with 300-second expiration.
   - Code verification uses constant-time string comparison (`constantTimeEquals`) to prevent timing side channels.
   - Rate limiting: 5 failed attempts trigger a 60-second lockout period.
2. **Explicit Host Approval:**
   - Client submission puts the request into `RequestPending`.
   - Host operator must explicitly approve the connection (`approvePairingRequest`).
   - Approved devices are recorded in `trusted_devices.json` with cryptographic public key fingerprints.
3. **Granular Permission Separation:**
   - Permissions are decoupled into separate capability flags:
     - `allowVideo`: Default enabled.
     - `allowAudio`: Default enabled.
     - `allowKeyboard`: Default disabled (requires host consent).
     - `allowMouse`: Default disabled (requires host consent).
     - `allowGamepad`: Default disabled (requires host consent).
   - Any device can have trust revoked instantly (`revokeDevice`), clearing all active sessions and held inputs.

## Consequences
- Frictionless pairing workflow without complex manual key exchanges.
- Robust defense against brute force and timing side channels.
- Strict isolation between media viewing and operating system input injection.
