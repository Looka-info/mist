//! Cryptographic primitives for Mist host-side authentication:
//! - Pure-Rust FIPS 180-4 standard SHA-256 implementation (zero external dependencies)
//! - Secure random token generation using OS-backed CSPRNG
//! - Hex encoding utilities

use rand::RngCore;

/// Pure-Rust SHA-256 digest computation (FIPS 180-4).
pub fn sha256(data: &[u8]) -> [u8; 32] {
    const K: [u32; 64] = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2,
    ];

    let mut h: [u32; 8] = [
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
        0x5be0cd19,
    ];

    let bit_len = (data.len() as u64) * 8;
    let mut msg = data.to_vec();
    msg.push(0x80);
    while (msg.len() % 64) != 56 {
        msg.push(0x00);
    }
    msg.extend_from_slice(&bit_len.to_be_bytes());

    #[allow(clippy::chunks_exact_to_as_chunks)]
    for chunk in msg.chunks_exact(64) {
        let mut w = [0u32; 64];
        for i in 0..16 {
            w[i] = u32::from_be_bytes([
                chunk[i * 4],
                chunk[i * 4 + 1],
                chunk[i * 4 + 2],
                chunk[i * 4 + 3],
            ]);
        }
        for i in 16..64 {
            let s0 = w[i - 15].rotate_right(7) ^ w[i - 15].rotate_right(18) ^ (w[i - 15] >> 3);
            let s1 = w[i - 2].rotate_right(17) ^ w[i - 2].rotate_right(19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16]
                .wrapping_add(s0)
                .wrapping_add(w[i - 7])
                .wrapping_add(s1);
        }

        let mut a = h[0];
        let mut b = h[1];
        let mut c = h[2];
        let mut d = h[3];
        let mut e = h[4];
        let mut f = h[5];
        let mut g = h[6];
        let mut h_var = h[7];

        for i in 0..64 {
            let s1 = e.rotate_right(6) ^ e.rotate_right(11) ^ e.rotate_right(25);
            let ch = (e & f) ^ ((!e) & g);
            let temp1 = h_var
                .wrapping_add(s1)
                .wrapping_add(ch)
                .wrapping_add(K[i])
                .wrapping_add(w[i]);
            let s0 = a.rotate_right(2) ^ a.rotate_right(13) ^ a.rotate_right(22);
            let maj = (a & b) ^ (a & c) ^ (b & c);
            let temp2 = s0.wrapping_add(maj);

            h_var = g;
            g = f;
            f = e;
            e = d.wrapping_add(temp1);
            d = c;
            c = b;
            b = a;
            a = temp1.wrapping_add(temp2);
        }

        h[0] = h[0].wrapping_add(a);
        h[1] = h[1].wrapping_add(b);
        h[2] = h[2].wrapping_add(c);
        h[3] = h[3].wrapping_add(d);
        h[4] = h[4].wrapping_add(e);
        h[5] = h[5].wrapping_add(f);
        h[6] = h[6].wrapping_add(g);
        h[7] = h[7].wrapping_add(h_var);
    }

    let mut result = [0u8; 32];
    for (i, val) in h.iter().enumerate() {
        result[i * 4..(i + 1) * 4].copy_from_slice(&val.to_be_bytes());
    }
    result
}

/// Compute SHA-256 and return as lowercase hexadecimal string.
pub fn sha256_hex(data: &[u8]) -> String {
    let digest = sha256(data);
    hex_encode(&digest)
}

/// Encode bytes into lowercase hexadecimal string.
pub fn hex_encode(bytes: &[u8]) -> String {
    const HEX_CHARS: &[u8] = b"0123456789abcdef";
    let mut s = String::with_capacity(bytes.len() * 2);
    for &b in bytes {
        s.push(HEX_CHARS[(b >> 4) as usize] as char);
        s.push(HEX_CHARS[(b & 0x0f) as usize] as char);
    }
    s
}

/// Generate cryptographically secure random bytes using OS-backed CSPRNG.
pub fn secure_random_bytes(len: usize) -> Vec<u8> {
    let mut bytes = vec![0u8; len];
    rand::thread_rng().fill_bytes(&mut bytes);
    bytes
}

/// Generate a high-entropy authorization or invitation token.
/// Example: `generate_secure_token("mist-auth", 32)` -> `mist-auth-3f9b...` (256-bit entropy).
pub fn generate_secure_token(prefix: &str, byte_len: usize) -> String {
    let raw = secure_random_bytes(byte_len);
    format!("{}-{}", prefix, hex_encode(&raw))
}

/// Constant-time byte slice comparison to defeat side-channel timing attacks.
pub fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }
    let mut diff = 0u8;
    for (x, y) in a.iter().zip(b.iter()) {
        diff |= x ^ y;
    }
    diff == 0
}

/// Normalizes a certificate fingerprint into uppercase hexadecimal characters with no colons or hyphens.
pub fn normalize_fingerprint(fp: &str) -> String {
    fp.chars()
        .filter(|c| *c != ':' && *c != ' ' && *c != '-')
        .flat_map(|c| c.to_uppercase())
        .collect()
}

/// Extracts the DTLS certificate fingerprint from an SDP description string.
pub fn extract_sdp_fingerprint(sdp: &str) -> Option<String> {
    for line in sdp.lines() {
        let trimmed = line.trim();
        if let Some(stripped) = trimmed.strip_prefix("a=fingerprint:") {
            let parts: Vec<&str> = stripped.split_whitespace().collect();
            if parts.len() >= 2 {
                return Some(normalize_fingerprint(parts[1]));
            }
        }
    }
    None
}

/// Computes a deterministic 6-digit Short Authentication String (SAS, e.g. "724-918")
/// from host and guest certificate fingerprints, session ID, and authorization token.
pub fn calculate_sas(host_fp: &str, guest_fp: &str, session_id: &str, auth_token: &str) -> String {
    let norm_host = normalize_fingerprint(host_fp);
    let norm_guest = normalize_fingerprint(guest_fp);
    let material = format!("{}|{}|{}|{}", norm_host, norm_guest, session_id, auth_token);

    let digest = sha256(material.as_bytes());
    let val = u32::from_be_bytes([digest[0], digest[1], digest[2], digest[3]]);
    let sas_num = val % 1_000_000;
    format!("{:03}-{:03}", sas_num / 1000, sas_num % 1000)
}

/// Standard HMAC-SHA256 (RFC 2104 / FIPS 198-1).
pub fn hmac_sha256(key: &[u8], message: &[u8]) -> [u8; 32] {
    let mut key_block = [0u8; 64];
    if key.len() > 64 {
        let hashed = sha256(key);
        key_block[..32].copy_from_slice(&hashed);
    } else {
        key_block[..key.len()].copy_from_slice(key);
    }

    let mut ipad = [0x36u8; 64];
    let mut opad = [0x5cu8; 64];
    for i in 0..64 {
        ipad[i] ^= key_block[i];
        opad[i] ^= key_block[i];
    }

    let mut inner_data = Vec::with_capacity(64 + message.len());
    inner_data.extend_from_slice(&ipad);
    inner_data.extend_from_slice(message);
    let inner_hash = sha256(&inner_data);

    let mut outer_data = Vec::with_capacity(64 + 32);
    outer_data.extend_from_slice(&opad);
    outer_data.extend_from_slice(&inner_hash);
    sha256(&outer_data)
}

/// Verifies HMAC-SHA256 in constant time.
pub fn verify_hmac(key: &[u8], message: &[u8], expected_tag: &[u8; 32]) -> bool {
    let tag = hmac_sha256(key, message);
    constant_time_eq(&tag, expected_tag)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sha256_nist_vectors() {
        // Test vector 1: empty string
        assert_eq!(
            sha256_hex(b""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        );

        // Test vector 2: "abc"
        assert_eq!(
            sha256_hex(b"abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );

        // Test vector 3: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
        assert_eq!(
            sha256_hex(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
        );
    }

    #[test]
    fn test_secure_token_entropy_and_prefix() {
        let tok1 = generate_secure_token("mist-auth", 32);
        let tok2 = generate_secure_token("mist-auth", 32);

        assert!(tok1.starts_with("mist-auth-"));
        assert!(tok2.starts_with("mist-auth-"));
        assert_ne!(tok1, tok2);
        // "mist-auth-" (10 chars) + 64 hex chars = 74 chars
        assert_eq!(tok1.len(), 10 + 64);
    }

    #[test]
    fn test_hmac_sha256_rfc4231() {
        // RFC 4231 Test Case 1
        let key = [0x0bu8; 20];
        let data = b"Hi There";
        let tag = hmac_sha256(&key, data);
        assert_eq!(
            hex_encode(&tag),
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"
        );
        assert!(verify_hmac(&key, data, &tag));
    }

    #[test]
    fn test_sas_computation_and_fingerprints() {
        let host_fp = "11:22:33:44:55:66:77:88";
        let guest_fp = "AA:BB:CC:DD:EE:FF:00:11";
        let session_id = "mist-sess-42";
        let token = "mist-auth-secret-123";

        let sas1 = calculate_sas(host_fp, guest_fp, session_id, token);
        let sas2 = calculate_sas(host_fp, guest_fp, session_id, token);

        assert_eq!(sas1.len(), 7);
        assert_eq!(&sas1[3..4], "-");
        assert_eq!(sas1, sas2);

        let altered = calculate_sas("99:99:99:99:55:66:77:88", guest_fp, session_id, token);
        assert_ne!(sas1, altered);
    }

    #[test]
    fn test_sdp_fingerprint_extraction() {
        let sdp = "v=0\r\no=- 123 2 IN IP4 127.0.0.1\r\na=fingerprint:sha-256 4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB:D5:08:05:3A:1E:0A:C4:4E:57:3E:45:9E\r\n";
        let fp = extract_sdp_fingerprint(sdp).unwrap();
        assert_eq!(
            fp,
            "4AADB9B13F82183B540212DF3E5D496B19E57CABD508053A1E0AC44E573E459E"
        );
    }
}
