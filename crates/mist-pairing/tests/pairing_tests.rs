use mist_domain::DevicePermissions;
use mist_pairing::{
    PairingManager, RemotePairingClient, RemotePairingRequest, RemotePairingResponse,
    RemotePairingServer, PAIRING_ALPHABET,
};
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::Duration;

#[test]
fn test_pairing_code_generation_and_alphabet() {
    let mut pm = PairingManager::new();
    let code = pm.generate_pairing_code("test-session-1".to_string());
    assert_eq!(code.len(), 6);
    for b in code.bytes() {
        assert!(PAIRING_ALPHABET.contains(&b), "Character not in alphabet: {}", b as char);
    }
}

#[test]
fn test_pairing_code_single_use() {
    let mut pm = PairingManager::new();
    let code = pm.generate_pairing_code("test-session-2".to_string());

    // First submission succeeds
    let res1 = pm.submit_request(
        "dev-client-1".to_string(),
        "Client PC".to_string(),
        &code,
        DevicePermissions::all(),
    );
    assert!(res1.is_ok());

    // Second submission of the same code fails (single-use)
    let res2 = pm.submit_request(
        "dev-client-2".to_string(),
        "Client PC 2".to_string(),
        &code,
        DevicePermissions::all(),
    );
    assert!(res2.is_err());
}

#[test]
fn test_pairing_rate_limit_lockout() {
    let mut pm = PairingManager::new();
    let _code = pm.generate_pairing_code("test-session-3".to_string());

    // 4 wrong attempts
    for i in 1..=4 {
        let res = pm.submit_request(
            "dev-attacker".to_string(),
            "Attacker PC".to_string(),
            "WRONG1",
            DevicePermissions::all(),
        );
        assert!(res.is_err(), "Attempt {} should fail", i);
        assert!(!pm.is_locked_out());
    }

    // 5th wrong attempt triggers lockout
    let res5 = pm.submit_request(
        "dev-attacker".to_string(),
        "Attacker PC".to_string(),
        "WRONG2",
        DevicePermissions::all(),
    );
    assert!(res5.is_err());
    assert!(pm.is_locked_out());
    assert!(pm.lockout_remaining_secs() > 0);

    // Further attempts are locked out
    let res6 = pm.submit_request(
        "dev-attacker".to_string(),
        "Attacker PC".to_string(),
        "WRONG3",
        DevicePermissions::all(),
    );
    assert!(res6.is_err());
    assert!(res6.unwrap_err().to_string().contains("Locked out"));
}

#[test]
fn test_host_approval_and_revocation() {
    let mut pm = PairingManager::new();
    let code = pm.generate_pairing_code("test-session-4".to_string());

    let req_id = pm
        .submit_request(
            "client-trusted-1".to_string(),
            "Trusted Laptop".to_string(),
            &code,
            DevicePermissions::all(),
        )
        .expect("Submission should succeed");

    assert_eq!(pm.pending_requests().len(), 1);

    // Host approves
    let approved = pm.approve_request(req_id, DevicePermissions::all());
    assert!(approved.is_some());
    let (auth_token, name) = approved.unwrap();
    assert!(auth_token.starts_with("mist-auth-"));
    assert_eq!(name, "Trusted Laptop");

    // Device is now in trusted devices
    let trusted = pm.trusted_devices();
    assert!(trusted.iter().any(|d| d.id.0 == "client-trusted-1"));

    // Host revokes
    assert!(pm.revoke_device("client-trusted-1"));
    let trusted_after = pm.trusted_devices();
    assert!(!trusted_after.iter().any(|d| d.id.0 == "client-trusted-1"));
}

#[test]
fn test_remote_pairing_tcp_flow() {
    let port = 8099;
    let pm = Arc::new(Mutex::new(PairingManager::new()));
    let code = pm.lock().unwrap().generate_pairing_code("sess-tcp-1".to_string());

    let decisions = Arc::new(Mutex::new(HashMap::new()));
    let decisions_clone = decisions.clone();
    let pm_clone = pm.clone();

    let mut server = RemotePairingServer::new();
    server
        .start(
            port,
            pm.clone(),
            Arc::new(move |req_id, _client_id, _client_name, _perms| {
                // Simulate host approving the request
                let (auth_tok, _) = pm_clone
                    .lock()
                    .unwrap()
                    .approve_request(req_id, DevicePermissions::all())
                    .unwrap();

                let resp = RemotePairingResponse {
                    success: true,
                    status: "approved".to_string(),
                    auth_token: auth_tok,
                    session_id: "sess-tcp-1".to_string(),
                    approved_permissions: 0x0F,
                    error_message: String::new(),
                    host_fingerprint: String::new(),
                    sas_code: String::new(),
                };
                decisions_clone.lock().unwrap().insert(req_id, Some(resp));
            }),
            decisions,
        )
        .expect("Server should start");

    std::thread::sleep(Duration::from_millis(100));

    let req = RemotePairingRequest::new(
        "test-client-tcp".to_string(),
        "TCP Client PC".to_string(),
        code,
        0x0F,
    );

    let resp = RemotePairingClient::submit_request("127.0.0.1", port, req, Duration::from_secs(5))
        .expect("Client should receive response");

    assert!(resp.success);
    assert_eq!(resp.status, "approved");
    assert!(resp.auth_token.starts_with("mist-auth-"));

    server.stop();
}

#[test]
fn test_temporary_invitation_tokens() {
    let mut pm = PairingManager::new();
    let session_id = "test-session-inv".to_string();

    // 1. Generate single-use invitation token
    let inv_token = pm.generate_invitation_token(
        session_id.clone(),
        10, // 10s TTL
        true,
        Some(DevicePermissions {
            keyboard: false,
            mouse: true,
            gamepad: false,
            audio: true,
            clipboard: false,
        }),
    );
    assert!(inv_token.starts_with("mist-inv-"));

    // 2. Submit with invitation token
    let req1 = RemotePairingRequest::new(
        "guest-laptop".to_string(),
        "Guest Laptop".to_string(),
        inv_token.clone(),
        0x0F, // Guest requests all, but invitation has preapproved
    );
    let req_id = pm.submit_pairing_request(&req1).expect("Invitation should be valid");
    assert_eq!(pm.pending_requests().len(), 1);

    // 3. Verify single-use consumption: submitting again fails
    let req2 = RemotePairingRequest::new(
        "guest-laptop-2".to_string(),
        "Guest Laptop 2".to_string(),
        inv_token.clone(),
        0x0F,
    );
    let res2 = pm.submit_pairing_request(&req2);
    assert!(res2.is_err(), "Single-use token should be consumed");

    // 4. Host approves req_id
    let approved = pm.approve_request(
        req_id,
        DevicePermissions {
            keyboard: false,
            mouse: true,
            gamepad: false,
            audio: true,
            clipboard: false,
        },
    );
    assert!(approved.is_some());
}

#[test]
fn test_session_bound_authorization() {
    let mut pm = PairingManager::new();
    let session_id = "sess-alpha-77".to_string();
    let code = pm.generate_pairing_code(session_id.clone());

    let req_id = pm
        .submit_request(
            "client-valid".to_string(),
            "Valid Guest PC".to_string(),
            &code,
            DevicePermissions::all(),
        )
        .expect("Submit should succeed");

    let host_perms = DevicePermissions {
        keyboard: true,
        mouse: false,
        gamepad: false,
        audio: true,
        clipboard: false,
    };
    let (auth_token, _) = pm.approve_request(req_id, host_perms.clone()).unwrap();

    // 1. Valid authentication against bound session & bound client
    let validated = pm.validate_session_auth(&session_id, "client-valid", &auth_token);
    assert!(validated.is_ok());
    assert_eq!(validated.unwrap(), host_perms);

    // 2. Session mismatch fails
    let wrong_session = pm.validate_session_auth("sess-beta-88", "client-valid", &auth_token);
    assert!(wrong_session.is_err());
    assert!(wrong_session.unwrap_err().to_string().contains("Session mismatch"));

    // 3. Client identity mismatch fails
    let wrong_client = pm.validate_session_auth(&session_id, "attacker-pc", &auth_token);
    assert!(wrong_client.is_err());
    assert!(wrong_client.unwrap_err().to_string().contains("Peer identity mismatch"));

    // 4. Revocation invalidates token
    assert!(pm.revoke_session_auth(&auth_token));
    let after_revoke = pm.validate_session_auth(&session_id, "client-valid", &auth_token);
    assert!(after_revoke.is_err());
}

#[test]
fn test_replay_protection_nonce_and_timestamp() {
    let mut pm = PairingManager::new();
    let code = pm.generate_pairing_code("sess-replay-1".to_string());

    let mut req = RemotePairingRequest::new(
        "client-replay".to_string(),
        "Replay Test PC".to_string(),
        code.clone(),
        0x0F,
    );
    let original_nonce = req.nonce.clone();

    // 1. First submission succeeds
    let res1 = pm.submit_pairing_request(&req);
    assert!(res1.is_ok());

    // 2. Same nonce submitted again triggers replay detection
    req.pairing_code = pm.generate_pairing_code("sess-replay-2".to_string());
    req.nonce = original_nonce; // Reused nonce
    let res2 = pm.submit_pairing_request(&req);
    assert!(res2.is_err());
    assert!(res2.unwrap_err().to_string().contains("Replay attack detected"));

    // 3. Expired timestamp (> 60s ago) triggers timestamp expiration
    let mut stale_req = RemotePairingRequest::new(
        "client-stale".to_string(),
        "Stale PC".to_string(),
        pm.generate_pairing_code("sess-replay-3".to_string()),
        0x0F,
    );
    stale_req.timestamp = (chrono::Utc::now().timestamp() - 120) as u64; // 2 minutes ago
    let res3 = pm.submit_pairing_request(&stale_req);
    assert!(res3.is_err());
    assert!(res3.unwrap_err().to_string().contains("expired"));
}

#[test]
fn test_guest_identity_records_and_permissions() {
    let mut pm = PairingManager::new();
    let session_id = "sess-records-1".to_string();
    let code = pm.generate_pairing_code(session_id.clone());

    let req_id = pm
        .submit_request(
            "guest-record-pc".to_string(),
            "Accounting Laptop".to_string(),
            &code,
            DevicePermissions::all(),
        )
        .unwrap();

    let host_perms = DevicePermissions {
        keyboard: false,
        mouse: false,
        gamepad: false,
        audio: true, // View/Listen only
        clipboard: false,
    };
    let (auth_tok, _) = pm.approve_request(req_id, host_perms.clone()).unwrap();

    // Verify guest identity record created
    let identities = pm.guest_identities();
    let guest = identities
        .iter()
        .find(|g| g.device_id.0 == "guest-record-pc")
        .expect("Guest record should exist");
    assert_eq!(guest.device_name, "Accounting Laptop");
    assert_eq!(guest.permissions, host_perms);
    assert_eq!(guest.connection_count, 0);

    // Authenticate session and check connection count increments
    let perms = pm
        .validate_session_auth(&session_id, "guest-record-pc", &auth_tok)
        .expect("Session auth should succeed");
    assert_eq!(perms, host_perms);

    let updated_identities = pm.guest_identities();
    let updated_guest = updated_identities
        .iter()
        .find(|g| g.device_id.0 == "guest-record-pc")
        .unwrap();
    assert_eq!(updated_guest.connection_count, 1);
}

#[test]
fn test_host_approval_lifecycle_deny_vs_kick_vs_ban() {
    let mut pm = PairingManager::new();
    let session_id = "sess-lifecycle-1".to_string();

    // Clean up any test device state
    pm.unban_device("test-client-lc");
    pm.revoke_device("test-client-lc");

    // 1. Submit request and DENY
    let code1 = pm.generate_pairing_code(session_id.clone());
    let req_id1 = pm
        .submit_request(
            "test-client-lc".to_string(),
            "Lifecycle Client".to_string(),
            &code1,
            DevicePermissions::all(),
        )
        .unwrap();

    assert_eq!(pm.pending_requests().len(), 1);
    assert!(pm.deny_request(req_id1));
    assert_eq!(pm.pending_requests().len(), 0);

    // Verify DENY is NOT a ban: client can submit again with a new code
    assert!(!pm.is_device_banned("test-client-lc"));
    let code2 = pm.generate_pairing_code(session_id.clone());
    let req_id2 = pm.submit_request(
        "test-client-lc".to_string(),
        "Lifecycle Client".to_string(),
        &code2,
        DevicePermissions::all(),
    );
    assert!(req_id2.is_ok(), "Denied client should be able to request again");

    // 2. APPROVE request
    let (auth_tok, _) = pm
        .approve_request(req_id2.unwrap(), DevicePermissions::all())
        .unwrap();
    assert!(pm.is_session_auth_valid(&session_id, "test-client-lc", &auth_tok));

    // 3. KICK guest: terminates active session, but does NOT ban or untrust
    assert!(pm.kick_guest("test-client-lc"));
    assert!(!pm.is_session_auth_valid(&session_id, "test-client-lc", &auth_tok));
    assert!(!pm.is_device_banned("test-client-lc"));

    // 4. BAN device: persistently blocks guest identity
    assert!(pm.ban_device("test-client-lc", Some("Lifecycle Client".to_string()), Some("Host policy violation".to_string())));
    assert!(pm.is_device_banned("test-client-lc"));

    // Banned device cannot submit pairing requests
    let code3 = pm.generate_pairing_code(session_id.clone());
    let req3 = pm.submit_request(
        "test-client-lc".to_string(),
        "Lifecycle Client".to_string(),
        &code3,
        DevicePermissions::all(),
    );
    assert!(req3.is_err());
    assert!(req3.unwrap_err().to_string().contains("banned"));

    // 5. UNBAN device: allows pairing again
    assert!(pm.unban_device("test-client-lc"));
    assert!(!pm.is_device_banned("test-client-lc"));

    let code4 = pm.generate_pairing_code(session_id.clone());
    let req4 = pm.submit_request(
        "test-client-lc".to_string(),
        "Lifecycle Client".to_string(),
        &code4,
        DevicePermissions::all(),
    );
    assert!(req4.is_ok());
}

#[test]
fn test_active_connected_guests_inspection() {
    let mut pm = PairingManager::new();
    let session_id = "sess-inspect-1".to_string();

    let code1 = pm.generate_pairing_code(session_id.clone());
    let req_id1 = pm
        .submit_request(
            "guest-dev-alpha".to_string(),
            "Alpha Device".to_string(),
            &code1,
            DevicePermissions::all(),
        )
        .unwrap();

    let (_tok_alpha, _) = pm.approve_request(req_id1, DevicePermissions::all()).unwrap();

    let code2 = pm.generate_pairing_code(session_id.clone());
    let req_id2 = pm
        .submit_request(
            "guest-dev-beta".to_string(),
            "Beta Device".to_string(),
            &code2,
            DevicePermissions {
                keyboard: false,
                mouse: false,
                gamepad: true,
                audio: true,
                clipboard: false,
            },
        )
        .unwrap();

    let (_tok_beta, _) = pm
        .approve_request(
            req_id2,
            DevicePermissions {
                keyboard: false,
                mouse: false,
                gamepad: true,
                audio: true,
                clipboard: false,
            },
        )
        .unwrap();

    // Inspect connected guests
    let active = pm.active_connected_guests();
    assert_eq!(active.len(), 2);
    assert!(active.iter().any(|g| g.device_id.0 == "guest-dev-alpha"));
    assert!(active.iter().any(|g| g.device_id.0 == "guest-dev-beta"));

    // Kick guest alpha
    assert!(pm.kick_guest("guest-dev-alpha"));

    let remaining = pm.active_connected_guests();
    assert_eq!(remaining.len(), 1);
    assert_eq!(remaining[0].device_id.0, "guest-dev-beta");
}

#[test]
fn test_per_guest_permission_models_and_clipboard() {
    let view_only = DevicePermissions::view_only();
    assert!(view_only.is_view_only());
    assert_eq!(view_only.to_mask(), 0x08); // Audio enabled, 0 input features

    let km = DevicePermissions::keyboard_and_mouse();
    assert!(!km.is_view_only());
    assert!(km.keyboard);
    assert!(km.mouse);
    assert!(!km.gamepad);
    assert!(!km.clipboard);
    assert_eq!(km.to_mask(), 0x01 | 0x02 | 0x08); // 0x0B

    let gp = DevicePermissions::gamepad_only();
    assert!(!gp.is_view_only());
    assert!(!gp.keyboard);
    assert!(!gp.mouse);
    assert!(gp.gamepad);
    assert_eq!(gp.to_mask(), 0x04 | 0x08); // 0x0C

    let all = DevicePermissions::all();
    assert_eq!(all.to_mask(), 0x01 | 0x02 | 0x04 | 0x08 | 0x10); // 0x1F

    let roundtrip = DevicePermissions::from_mask(0x1F);
    assert!(roundtrip.keyboard);
    assert!(roundtrip.mouse);
    assert!(roundtrip.gamepad);
    assert!(roundtrip.audio);
    assert!(roundtrip.clipboard);
}

#[test]
fn test_runtime_permission_changes_and_immediate_revocation() {
    let mut pm = PairingManager::new();
    let session_id = "sess-perm-rt".to_string();

    let code = pm.generate_pairing_code(session_id.clone());
    let req_id = pm
        .submit_request(
            "guest-perm-client".to_string(),
            "Perm Client".to_string(),
            &code,
            DevicePermissions::all(),
        )
        .unwrap();

    // 1. Host grants all permissions initially
    let (_auth_tok, _) = pm.approve_request(req_id, DevicePermissions::all()).unwrap();

    // Host input layer verification
    assert!(pm.verify_guest_action("guest-perm-client", "keyboard"));
    assert!(pm.verify_guest_action("guest-perm-client", "mouse"));
    assert!(pm.verify_guest_action("guest-perm-client", "gamepad"));
    assert!(pm.verify_guest_action("guest-perm-client", "clipboard"));

    // 2. Runtime permission change: switch guest to Gamepad-only
    let gp_perms = DevicePermissions::gamepad_only();
    assert!(pm.update_device_permissions("guest-perm-client", gp_perms));

    // Keyboard, mouse, and clipboard immediately rejected
    assert!(!pm.verify_guest_action("guest-perm-client", "keyboard"));
    assert!(!pm.verify_guest_action("guest-perm-client", "mouse"));
    assert!(!pm.verify_guest_action("guest-perm-client", "clipboard"));
    // Gamepad remains allowed
    assert!(pm.verify_guest_action("guest-perm-client", "gamepad"));

    // 3. Immediate revocation to View-Only
    assert!(pm.revoke_guest_permissions("guest-perm-client"));
    assert!(!pm.verify_guest_action("guest-perm-client", "gamepad"));
    assert!(!pm.verify_guest_action("guest-perm-client", "keyboard"));
    assert!(!pm.verify_guest_action("guest-perm-client", "mouse"));
    assert!(!pm.verify_guest_action("guest-perm-client", "clipboard"));

    // 4. Permission audit log contains records
    let log = pm.permission_audit_log();
    assert!(!log.is_empty());
    assert!(log.iter().any(|e| e.action == "updated"));
    assert!(log.iter().any(|e| e.action == "revoked_to_view_only"));
    assert!(log.iter().any(|e| !e.allowed));
}

#[test]
fn test_encryption_verification_and_sas_derivation() {
    let mut pm = PairingManager::default();
    let code = pm.generate_pairing_code("sess-tls-1".to_string());

    let client_fp = "4A:AD:B9:B1:3F:82:18:3B:54:02:12:DF:3E:5D:49:6B:19:E5:7C:AB:D5:08:05:3A:1E:0A:C4:4E:57:3E:45:9E";
    let host_fp = "11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00";

    let req = RemotePairingRequest::new(
        "tls-guest-pc".to_string(),
        "TLS Guest Device".to_string(),
        code,
        0x0F,
    )
    .with_fingerprint(client_fp);

    let req_id = pm.submit_pairing_request(&req).expect("Request submission should succeed");

    // Approve with host fingerprint
    let (auth_token, client_name, host_sas) = pm
        .approve_request_with_sas(req_id, DevicePermissions::all(), host_fp)
        .expect("Approval should succeed");

    assert_eq!(client_name, "TLS Guest Device");
    assert!(!host_sas.is_empty());
    assert_eq!(host_sas.len(), 7); // XXX-YYY
    assert_eq!(&host_sas[3..4], "-");

    // Client independently calculates SAS with identical parameters
    let guest_sas = mist_pairing::crypto::calculate_sas(host_fp, client_fp, "sess-tls-1", &auth_token);
    assert_eq!(host_sas, guest_sas, "Host and guest SAS codes must match exactly for verification");

    // Validate session authentication with matching fingerprint
    let val_res = pm.validate_session_auth_with_fingerprint(
        "sess-tls-1",
        "tls-guest-pc",
        &auth_token,
        client_fp,
    );
    assert!(val_res.is_ok(), "Authentication with matching DTLS fingerprint must succeed");
}

#[test]
fn test_dtls_fingerprint_mismatch_rejected() {
    let mut pm = PairingManager::default();
    let code = pm.generate_pairing_code("sess-tls-mitm".to_string());

    let legitimate_fp = "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    let attacker_fp = "FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00:FF:EE:DD:CC:BB:AA:99:88:77:66:55:44:33:22:11:00";
    let host_fp = "12:34:56:78:90:AB:CD:EF:12:34:56:78:90:AB:CD:EF:12:34:56:78:90:AB:CD:EF:12:34:56:78:90:AB:CD:EF";

    let req = RemotePairingRequest::new(
        "victim-device".to_string(),
        "Victim Device".to_string(),
        code,
        0x0F,
    )
    .with_fingerprint(legitimate_fp);

    let req_id = pm.submit_pairing_request(&req).unwrap();
    let (auth_token, _, _) = pm
        .approve_request_with_sas(req_id, DevicePermissions::all(), host_fp)
        .unwrap();

    // Attacker intercepts token but presents a different DTLS certificate fingerprint
    let spoof_attempt = pm.validate_session_auth_with_fingerprint(
        "sess-tls-mitm",
        "victim-device",
        &auth_token,
        attacker_fp,
    );
    assert!(spoof_attempt.is_err(), "Authentication with mismatched DTLS fingerprint must be rejected");
    let err_str = spoof_attempt.unwrap_err().to_string();
    assert!(err_str.contains("Peer identity mismatch"), "Error should explain fingerprint mismatch: {}", err_str);
}
