# ADR 0034: Release Code Signing and Update Integrity Verification

## Status
Accepted

## Context
Windows SmartScreen and security software require that distributed binaries carry authentic cryptographic signatures and tamper-evident hashes. Additionally, development and CI pipelines must be able to verify binary integrity without requiring production private keys stored in the public repository.

## Decision
1. **Separation of Production and Development Signing:**
   - Production signing certificates and PFX files are strictly excluded from the repository.
   - `scripts/sign-release.ps1` checks for an existing Code Signing certificate in `Cert:\CurrentUser\My`. If absent in development, it generates an ephemeral self-signed Authenticode certificate for local integrity testing.
   - Production CI uses environment secrets or hardware security modules (HSM) with `signtool.exe` and RFC 3161 timestamping (`http://timestamp.digicert.com`).
2. **Cryptographic Release Manifest (`release-manifest.json`):**
   - Every distribution build generates a signed manifest containing:
     - Filename and relative path.
     - Exact file size in bytes.
     - SHA-256 cryptographic hash.
     - Authenticode signature status.
     - Certificate thumbprint.
3. **Integrity Verification:**
   - Update mechanisms and users can independently verify binary checksums against the manifest prior to executing updates.

## Consequences
- Zero private keys or credentials checked into source control.
- Tamper-evident binary verification across all distributed components.
- Automated pipeline validation in both development and production CI.
