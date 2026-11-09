# Mist Security & Trust Model

## 1. Overview

Mist employs a zero-trust, host-authoritative security model designed to provide low-latency game streaming without compromising user device security or cryptographic integrity.

Every session establishes authenticated, end-to-end encrypted peer transport with constant-time cryptographic verification and out-of-band identity validation.

---

## 2. Core Cryptographic Components

### 2.1 Transport Layer Security (DTLS-SRTP)
- All real-time media streams (video, audio) and data channels (input, control, clipboard) are encrypted using **DTLS 1.2 / 1.3** and **SRTP** with AEAD ciphers (`AEAD_AES_128_GCM` or `AEAD_AES_256_GCM`).
- Ephemeral DTLS certificates are generated per application process lifecycle.
- Certificate fingerprints are SHA-256 digests formatted as 32 hex pairs.

### 2.2 Host-Authoritative Token Generation
- Authorization tokens are generated using a Cryptographically Secure Pseudo-Random Number Generator (CSPRNG, `BCryptGenRandom` on Windows / `getrandom`).
- Token entropy: 256 bits (64 hex characters), prefixed with `mist-auth-`.
- Tokens are stored in host memory and persistent identity ledgers strictly as SHA-256 digests (`crypto::sha256_hex`). Plaintext tokens are never stored to disk.

### 2.3 Short Authentication String (SAS)
To protect against active Man-in-the-Middle (MitM) attacks during signaling or SDP exchange, Mist derives a 6-digit Short Authentication String formatted as `XXX-YYY`:

$$\text{Material} = \text{NormHostFP} \parallel \text{"|"} \parallel \text{NormGuestFP} \parallel \text{"|"} \parallel \text{SessionID} \parallel \text{"|"} \parallel \text{AuthToken}$$

$$\text{Digest} = \text{SHA-256}(\text{Material})$$

$$\text{SAS\_Value} = \text{BigEndianU32}(\text{Digest}[0..4]) \pmod{1\,000\,000}$$

Both the host and guest compute this SAS independently. Users can compare the 6-digit code visually out-of-band to guarantee that no intermediary has tampered with SDP offers or certificates.

---

## 3. Trust State Machine

The connection lifecycle transitions through explicit cryptographic verification states:

```
[ Idle / Unverified ]
         │
         ▼ (SDP Offer / Answer Received)
[ PeerIdentityVerifier::verifyRemoteDescription ]
         │
         ├──► Normalized Fingerprint Matches Paired Identity?
         │         │
         │         ├── YES ──► [ Verified ] (Allow PeerConnection to proceed)
         │         │
         │         └── NO  ──► [ FingerprintMismatch ] (Abort negotiation; disconnect)
         │
         ▼ (In-Flight Renegotiation / SDP Update)
[ Detect In-Flight Fingerprint Drift ]
         │
         ├── Unchanged ──► Remain in [ Verified ]
         └── Changed   ──► [ IdentityChanged ] (Immediate session termination; security alert)
```

### 3.1 Verification States
- `Unverified`: Initial state prior to SDP remote description verification.
- `Verified`: Certificate fingerprint matched the pairing identity; DTLS handshake permitted.
- `FingerprintMismatch`: The remote peer presented an SDP certificate fingerprint differing from the authenticated pairing record.
- `IdentityChanged`: An established connection attempted in-flight certificate swap without re-pairing (potential session hijacking or relay interception).

---

## 4. Threat Matrix & Mitigations

| Threat | Attack Vector | Mist Mitigation |
|---|---|---|
| **Passive Eavesdropping** | Packet capture on local LAN or public Internet | DTLS-SRTP AEAD encryption on all media and data packets. |
| **Active Man-in-the-Middle** | Rogue TURN relay or signaling server modifying SDP | DTLS fingerprint bound during pairing; out-of-band 6-digit SAS code comparison. |
| **Replay Attacks** | Re-sending captured pairing packets | Replay defense: CSPRNG 128-bit nonces with single-use cache, plus strict $\pm 60$s UTC timestamp skew check. |
| **Brute-Force Pairing** | Enumerating 6-character alphanumeric codes | 5-attempt threshold triggers a mandatory 60-second lockout. Single-use code invalidation upon first valid submission. |
| **Session Hijacking** | Reusing a leaked auth token from a different device | Tokens are cryptographically bound to specific `client_device_id` and verified against DTLS certificate fingerprint in constant time. |
| **Privilege Escalation** | Modified client claiming host permissions | Host-authoritative permission enforcement: host ignores client-claimed masks and verifies every keystroke/mouse action against host memory state. |
| **Timing Side-Channels** | Measuring token / fingerprint string comparison time | All cryptographic hashes, tokens, and fingerprints use constant-time memory comparison (`crypto::constant_time_eq`). |

---

## 5. Persistence and Key Storage

- **Guest Identities Ledger:** `%APPDATA%/Mist/guest_identities.json` stores SHA-256 token digests, device IDs, granted permission masks, and timestamps.
- **Banned Devices Ledger:** `%APPDATA%/Mist/banned_devices.json` stores blacklisted device identifiers. Banned devices are rejected immediately at TCP pairing and signaling boundaries.
- **Temporary Sessions:** Invitation tokens (`mist-inv-`) expire automatically based on configured TTL (default 15 minutes) and are immediately consumed upon first use.
