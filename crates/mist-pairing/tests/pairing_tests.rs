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
    assert!(auth_token.starts_with("auth-tok-"));
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
                };
                decisions_clone.lock().unwrap().insert(req_id, Some(resp));
            }),
            decisions,
        )
        .expect("Server should start");

    std::thread::sleep(Duration::from_millis(100));

    let req = RemotePairingRequest {
        client_device_id: "test-client-tcp".to_string(),
        client_device_name: "TCP Client PC".to_string(),
        pairing_code: code,
        requested_permissions: 0x0F,
    };

    let resp = RemotePairingClient::submit_request("127.0.0.1", port, req, Duration::from_secs(5))
        .expect("Client should receive response");

    assert!(resp.success);
    assert_eq!(resp.status, "approved");
    assert!(resp.auth_token.starts_with("auth-tok-"));

    server.stop();
}
