//! Pairing logic, rate limiting, host authorization, and remote pairing service.

pub mod crypto;

use chrono::Utc;
use mist_core::{MistError, Result};
use mist_domain::{
    AuthorizedSessionTicket, DeviceEntry, DeviceId, DevicePermissions, GuestIdentityRecord,
};
use rand::seq::SliceRandom;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

pub const PAIRING_ALPHABET: &[u8] = b"23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
pub const PAIRING_CODE_LENGTH: usize = 6;
pub const PAIRING_CODE_TTL_SECS: u64 = 300; // 5 minutes
pub const MAX_FAILED_ATTEMPTS: u32 = 5;
pub const LOCKOUT_DURATION_SECS: u64 = 60; // 60 seconds
pub const REPLAY_WINDOW_SECS: u64 = 60; // 60 seconds allowed skew / replay protection window

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemotePairingRequest {
    pub client_device_id: String,
    pub client_device_name: String,
    pub pairing_code: String,
    pub requested_permissions: u32,
    #[serde(default)]
    pub nonce: String,
    #[serde(default)]
    pub timestamp: u64,
}

impl Default for RemotePairingRequest {
    fn default() -> Self {
        Self {
            client_device_id: String::new(),
            client_device_name: String::new(),
            pairing_code: String::new(),
            requested_permissions: 0,
            nonce: String::new(),
            timestamp: 0,
        }
    }
}

impl RemotePairingRequest {
    pub fn new(
        client_device_id: String,
        client_device_name: String,
        pairing_code: String,
        requested_permissions: u32,
    ) -> Self {
        let nonce_bytes = crypto::secure_random_bytes(16);
        let nonce = crypto::hex_encode(&nonce_bytes);
        let timestamp = Utc::now().timestamp() as u64;
        Self {
            client_device_id,
            client_device_name,
            pairing_code,
            requested_permissions,
            nonce,
            timestamp,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemotePairingResponse {
    pub success: bool,
    pub status: String,
    pub auth_token: String,
    pub session_id: String,
    pub approved_permissions: u32,
    pub error_message: String,
}

#[derive(Debug, Clone)]
pub struct PendingRequest {
    pub request_id: u64,
    pub client_device_id: String,
    pub client_device_name: String,
    pub session_id: String,
    pub requested_permissions: DevicePermissions,
    pub timestamp: Instant,
}

#[derive(Debug, Clone)]
struct ActiveCode {
    code: String,
    created_at: Instant,
    session_id: String,
}

#[derive(Debug, Clone)]
pub struct InvitationToken {
    pub token: String,
    pub token_hash: String,
    pub session_id: String,
    pub created_at: Instant,
    pub ttl_secs: u64,
    pub single_use: bool,
    pub preapproved_permissions: Option<DevicePermissions>,
}

pub struct PairingManager {
    active_code: Option<ActiveCode>,
    invitation_tokens: HashMap<String, InvitationToken>,
    failed_attempts: u32,
    lockout_until: Option<Instant>,
    trusted_devices: HashMap<String, DeviceEntry>,
    guest_identities: HashMap<String, GuestIdentityRecord>,
    authorized_tickets: HashMap<String, AuthorizedSessionTicket>,
    seen_nonces: HashMap<String, Instant>,
    pending_requests: Vec<PendingRequest>,
    next_request_id: u64,
}

impl Default for PairingManager {
    fn default() -> Self {
        let mut mgr = Self {
            active_code: None,
            invitation_tokens: HashMap::new(),
            failed_attempts: 0,
            lockout_until: None,
            trusted_devices: HashMap::new(),
            guest_identities: HashMap::new(),
            authorized_tickets: HashMap::new(),
            seen_nonces: HashMap::new(),
            pending_requests: Vec::new(),
            next_request_id: 1,
        };
        mgr.load_trusted_devices();
        mgr.load_guest_identities();
        mgr
    }
}

impl PairingManager {
    pub fn new() -> Self {
        Self::default()
    }

    fn storage_dir() -> PathBuf {
        if let Ok(appdata) = std::env::var("APPDATA") {
            let p = Path::new(&appdata).join("Mist");
            let _ = fs::create_dir_all(&p);
            p
        } else {
            PathBuf::from(".")
        }
    }

    fn trusted_devices_path() -> PathBuf {
        Self::storage_dir().join("trusted_devices.json")
    }

    fn guest_identities_path() -> PathBuf {
        Self::storage_dir().join("guest_identities.json")
    }

    fn load_trusted_devices(&mut self) {
        let p = Self::trusted_devices_path();
        if p.exists() {
            if let Ok(content) = fs::read_to_string(&p) {
                if let Ok(devices) = serde_json::from_str::<Vec<DeviceEntry>>(&content) {
                    for d in devices {
                        self.trusted_devices.insert(d.id.0.clone(), d);
                    }
                }
            }
        }
    }

    pub fn save_trusted_devices(&self) {
        let p = Self::trusted_devices_path();
        let devices: Vec<&DeviceEntry> = self.trusted_devices.values().collect();
        if let Ok(content) = serde_json::to_string_pretty(&devices) {
            let _ = fs::write(&p, content);
        }
    }

    fn load_guest_identities(&mut self) {
        let p = Self::guest_identities_path();
        if p.exists() {
            if let Ok(content) = fs::read_to_string(&p) {
                if let Ok(records) = serde_json::from_str::<Vec<GuestIdentityRecord>>(&content) {
                    for r in records {
                        self.guest_identities.insert(r.device_id.0.clone(), r);
                    }
                }
            }
        }
    }

    pub fn save_guest_identities(&self) {
        let p = Self::guest_identities_path();
        let records: Vec<&GuestIdentityRecord> = self.guest_identities.values().collect();
        if let Ok(content) = serde_json::to_string_pretty(&records) {
            let _ = fs::write(&p, content);
        }
    }

    pub fn is_locked_out(&self) -> bool {
        if let Some(until) = self.lockout_until {
            if Instant::now() < until {
                return true;
            }
        }
        false
    }

    pub fn lockout_remaining_secs(&self) -> u64 {
        if let Some(until) = self.lockout_until {
            let now = Instant::now();
            if now < until {
                return (until - now).as_secs();
            }
        }
        0
    }

    /// Generates a 6-character human-readable pairing code.
    pub fn generate_pairing_code(&mut self, session_id: String) -> String {
        let mut rng = rand::thread_rng();
        let code_chars: Vec<u8> = (0..PAIRING_CODE_LENGTH)
            .map(|_| *PAIRING_ALPHABET.choose(&mut rng).unwrap())
            .collect();
        let code = String::from_utf8(code_chars).unwrap();

        self.active_code = Some(ActiveCode {
            code: code.clone(),
            created_at: Instant::now(),
            session_id,
        });
        self.failed_attempts = 0;
        self.lockout_until = None;

        code
    }

    /// Generates a high-entropy temporary invitation token (256-bit CSPRNG).
    pub fn generate_invitation_token(
        &mut self,
        session_id: String,
        ttl_secs: u64,
        single_use: bool,
        preapproved_permissions: Option<DevicePermissions>,
    ) -> String {
        let token = crypto::generate_secure_token("mist-inv", 32);
        let token_hash = crypto::sha256_hex(token.as_bytes());

        self.invitation_tokens.insert(
            token.clone(),
            InvitationToken {
                token: token.clone(),
                token_hash,
                session_id,
                created_at: Instant::now(),
                ttl_secs,
                single_use,
                preapproved_permissions,
            },
        );

        token
    }

    pub fn revoke_invitation(&mut self, token: &str) -> bool {
        self.invitation_tokens.remove(token).is_some()
    }

    pub fn active_code(&self) -> Option<String> {
        if let Some(ref c) = self.active_code {
            if c.created_at.elapsed().as_secs() < PAIRING_CODE_TTL_SECS {
                return Some(c.code.clone());
            }
        }
        None
    }

    pub fn active_session_id(&self) -> Option<&str> {
        if let Some(ref c) = self.active_code {
            if c.created_at.elapsed().as_secs() < PAIRING_CODE_TTL_SECS {
                return Some(&c.session_id);
            }
        }
        None
    }

    pub fn active_code_remaining_secs(&self) -> u64 {
        if let Some(ref c) = self.active_code {
            let elapsed = c.created_at.elapsed().as_secs();
            if elapsed < PAIRING_CODE_TTL_SECS {
                return PAIRING_CODE_TTL_SECS - elapsed;
            }
        }
        0
    }

    pub fn revoke_code(&mut self) {
        self.active_code = None;
    }

    /// Verifies replay protection against nonce reuse and stale timestamp skew.
    fn verify_replay(&mut self, nonce: &str, timestamp: u64) -> Result<()> {
        if nonce.is_empty() {
            // Legacy / simple client without nonce
            return Ok(());
        }

        // Clean up stale nonces older than 2x window
        let cutoff = Instant::now() - Duration::from_secs(REPLAY_WINDOW_SECS * 2);
        self.seen_nonces.retain(|_, seen_at| *seen_at > cutoff);

        // Check timestamp window if provided
        if timestamp > 0 {
            let now = Utc::now().timestamp() as u64;
            if now > timestamp + REPLAY_WINDOW_SECS {
                return Err(MistError::Pairing(
                    "Pairing request expired: timestamp is older than allowable replay window."
                        .to_string(),
                ));
            }
            if timestamp > now + 30 {
                return Err(MistError::Pairing(
                    "Pairing request rejected: client clock is significantly ahead of host clock."
                        .to_string(),
                ));
            }
        }

        // Check nonce uniqueness
        if self.seen_nonces.contains_key(nonce) {
            return Err(MistError::Pairing(
                "Replay attack detected: pairing nonce has already been consumed.".to_string(),
            ));
        }

        self.seen_nonces.insert(nonce.to_string(), Instant::now());
        Ok(())
    }

    /// Submits a pairing request with complete replay defense and invitation token support.
    pub fn submit_pairing_request(&mut self, req: &RemotePairingRequest) -> Result<u64> {
        if self.is_locked_out() {
            let remaining = self.lockout_remaining_secs();
            return Err(MistError::Pairing(format!(
                "Too many failed attempts. Locked out for {} seconds.",
                remaining
            )));
        }

        // Replay defense
        self.verify_replay(&req.nonce, req.timestamp)?;

        // Clean up expired code
        if let Some(ref c) = self.active_code {
            if c.created_at.elapsed().as_secs() >= PAIRING_CODE_TTL_SECS {
                self.active_code = None;
            }
        }

        let input_code = req.pairing_code.trim();

        // 1. Check against active 6-character pairing code
        let mut matched_session_id = None;
        let mut preapproved = None;

        if let Some(ref c) = self.active_code {
            if c.code.eq_ignore_ascii_case(input_code) {
                matched_session_id = Some(c.session_id.clone());
                // Single-use: consume code immediately
                self.active_code = None;
            }
        }

        // 2. Check against invitation tokens
        if matched_session_id.is_none() {
            if let Some(inv) = self.invitation_tokens.get(input_code) {
                if inv.created_at.elapsed().as_secs() < inv.ttl_secs {
                    matched_session_id = Some(inv.session_id.clone());
                    preapproved = inv.preapproved_permissions.clone();
                    if inv.single_use {
                        self.invitation_tokens.remove(input_code);
                    }
                }
            }
        }

        if matched_session_id.is_none() {
            self.failed_attempts += 1;
            if self.failed_attempts >= MAX_FAILED_ATTEMPTS {
                self.lockout_until =
                    Some(Instant::now() + Duration::from_secs(LOCKOUT_DURATION_SECS));
                tracing::warn!("Pairing rate limit reached. Locked out for 60 seconds.");
                return Err(MistError::Pairing(
                    "Too many failed attempts. Locked out for 60 seconds.".to_string(),
                ));
            }
            return Err(MistError::Pairing(format!(
                "Invalid pairing code or invitation token. {} attempt(s) remaining.",
                MAX_FAILED_ATTEMPTS - self.failed_attempts
            )));
        }

        let session_id = matched_session_id.unwrap();
        self.failed_attempts = 0;

        let req_id = self.next_request_id;
        self.next_request_id += 1;

        let requested_permissions = if let Some(pre) = preapproved {
            pre
        } else {
            DevicePermissions::from_mask(req.requested_permissions)
        };

        self.pending_requests.push(PendingRequest {
            request_id: req_id,
            client_device_id: req.client_device_id.clone(),
            client_device_name: req.client_device_name.clone(),
            session_id,
            requested_permissions,
            timestamp: Instant::now(),
        });

        Ok(req_id)
    }

    /// Backwards-compatible submission entry point.
    pub fn submit_request(
        &mut self,
        device_id: String,
        device_name: String,
        code: &str,
        requested_permissions: DevicePermissions,
    ) -> Result<u64> {
        let req = RemotePairingRequest::new(
            device_id,
            device_name,
            code.to_string(),
            requested_permissions.to_mask(),
        );
        self.submit_pairing_request(&req)
    }

    pub fn pending_requests(&self) -> &[PendingRequest] {
        &self.pending_requests
    }

    /// Approves pending pairing request, issues a 256-bit cryptographic auth token,
    /// creates a session ticket bound to the approved session/peer identity, and persists guest records.
    pub fn approve_request(
        &mut self,
        request_id: u64,
        approved_permissions: DevicePermissions,
    ) -> Option<(String, String)> {
        if let Some(idx) = self.pending_requests.iter().position(|r| r.request_id == request_id) {
            let req = self.pending_requests.remove(idx);

            // Generate cryptographically secure 256-bit token
            let auth_token = crypto::generate_secure_token("mist-auth", 32);
            let token_hash = crypto::sha256_hex(auth_token.as_bytes());

            // Bind session ticket to session_id and client_device_id
            let now = Utc::now();
            let expires_at = (now + chrono::Duration::hours(24)).to_rfc3339();

            let ticket = AuthorizedSessionTicket {
                auth_token_hash: token_hash.clone(),
                session_id: req.session_id.clone(),
                client_device_id: req.client_device_id.clone(),
                client_device_name: req.client_device_name.clone(),
                issued_at: now.to_rfc3339(),
                expires_at,
                permissions: approved_permissions.clone(),
                consumed: false,
            };
            self.authorized_tickets.insert(token_hash.clone(), ticket);

            // Update or create guest identity record
            let dev_id_str = req.client_device_id.clone();
            let guest = self
                .guest_identities
                .entry(dev_id_str.clone())
                .or_insert_with(|| GuestIdentityRecord {
                    device_id: DeviceId::new(&dev_id_str),
                    device_name: req.client_device_name.clone(),
                    first_seen: now.to_rfc3339(),
                    last_authenticated: now.to_rfc3339(),
                    connection_count: 0,
                    permissions: approved_permissions.clone(),
                    is_trusted: true,
                    auth_token_hash: token_hash.clone(),
                });
            guest.device_name = req.client_device_name.clone();
            guest.last_authenticated = now.to_rfc3339();
            guest.permissions = approved_permissions.clone();
            guest.is_trusted = true;
            guest.auth_token_hash = token_hash;

            // Update trusted devices list
            let entry = DeviceEntry {
                id: DeviceId::new(&req.client_device_id),
                name: req.client_device_name.clone(),
                last_connected: now.to_rfc3339(),
                permissions: approved_permissions,
                is_trusted: true,
            };
            self.trusted_devices.insert(req.client_device_id, entry);

            self.save_trusted_devices();
            self.save_guest_identities();

            Some((auth_token, req.client_device_name))
        } else {
            None
        }
    }

    /// Host-side validation of incoming session authentication.
    /// Verifies token authenticity, expiration, session binding, and client device binding.
    /// Returns the authoritative host-granted permissions (never untrusted guest claims).
    pub fn validate_session_auth(
        &mut self,
        session_id: &str,
        client_device_id: &str,
        auth_token: &str,
    ) -> Result<DevicePermissions> {
        let hash = crypto::sha256_hex(auth_token.as_bytes());
        let permissions = {
            let ticket = self
                .authorized_tickets
                .get_mut(&hash)
                .ok_or_else(|| MistError::Pairing("Authentication token not recognized or revoked.".to_string()))?;

            if ticket.session_id != session_id {
                return Err(MistError::Pairing(format!(
                    "Session mismatch: token is bound to session '{}', not '{}'.",
                    ticket.session_id, session_id
                )));
            }

            if ticket.client_device_id != client_device_id {
                return Err(MistError::Pairing(format!(
                    "Peer identity mismatch: token is bound to device '{}', not '{}'.",
                    ticket.client_device_id, client_device_id
                )));
            }

            if let Ok(exp) = chrono::DateTime::parse_from_rfc3339(&ticket.expires_at) {
                if Utc::now() > exp {
                    return Err(MistError::Pairing("Authentication token has expired.".to_string()));
                }
            }

            ticket.permissions.clone()
        };

        // Record authenticated connection
        if let Some(record) = self.guest_identities.get_mut(client_device_id) {
            record.last_authenticated = Utc::now().to_rfc3339();
            record.connection_count += 1;
            record.auth_token_hash = hash;
            self.save_guest_identities();
        }

        Ok(permissions)
    }

    /// Revokes an active session authorization token.
    pub fn revoke_session_auth(&mut self, auth_token: &str) -> bool {
        let hash = crypto::sha256_hex(auth_token.as_bytes());
        self.authorized_tickets.remove(&hash).is_some()
    }

    pub fn is_session_auth_valid(
        &self,
        session_id: &str,
        client_device_id: &str,
        auth_token: &str,
    ) -> bool {
        let hash = crypto::sha256_hex(auth_token.as_bytes());
        if let Some(ticket) = self.authorized_tickets.get(&hash) {
            if ticket.session_id == session_id && ticket.client_device_id == client_device_id {
                if let Ok(exp) = chrono::DateTime::parse_from_rfc3339(&ticket.expires_at) {
                    return Utc::now() <= exp;
                }
            }
        }
        false
    }

    pub fn guest_identities(&self) -> Vec<GuestIdentityRecord> {
        let mut list: Vec<GuestIdentityRecord> = self.guest_identities.values().cloned().collect();
        list.sort_by(|a, b| b.last_authenticated.cmp(&a.last_authenticated));
        list
    }

    pub fn deny_request(&mut self, request_id: u64) -> bool {
        if let Some(idx) = self.pending_requests.iter().position(|r| r.request_id == request_id) {
            self.pending_requests.remove(idx);
            true
        } else {
            false
        }
    }

    pub fn trusted_devices(&self) -> Vec<DeviceEntry> {
        let mut list: Vec<DeviceEntry> = self.trusted_devices.values().cloned().collect();
        list.sort_by(|a, b| b.last_connected.cmp(&a.last_connected));
        list
    }

    pub fn revoke_device(&mut self, device_id: &str) -> bool {
        let res = self.trusted_devices.remove(device_id).is_some();
        if let Some(record) = self.guest_identities.get_mut(device_id) {
            record.is_trusted = false;
        }
        // Invalidate any active session tickets for this device
        self.authorized_tickets
            .retain(|_, ticket| ticket.client_device_id != device_id);

        if res {
            self.save_trusted_devices();
            self.save_guest_identities();
        }
        res
    }

    pub fn update_device_permissions(&mut self, device_id: &str, perms: DevicePermissions) -> bool {
        let mut updated = false;
        if let Some(dev) = self.trusted_devices.get_mut(device_id) {
            dev.permissions = perms.clone();
            updated = true;
        }
        if let Some(record) = self.guest_identities.get_mut(device_id) {
            record.permissions = perms;
            updated = true;
        }
        if updated {
            self.save_trusted_devices();
            self.save_guest_identities();
        }
        updated
    }
}

pub struct RemotePairingServer {
    is_running: Arc<Mutex<bool>>,
}

impl Default for RemotePairingServer {
    fn default() -> Self {
        Self {
            is_running: Arc::new(Mutex::new(false)),
        }
    }
}

impl RemotePairingServer {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn start(
        &mut self,
        port: u16,
        manager: Arc<Mutex<PairingManager>>,
        approval_cb: Arc<dyn Fn(u64, String, String, DevicePermissions) + Send + Sync>,
        decision_receiver: Arc<Mutex<HashMap<u64, Option<RemotePairingResponse>>>>,
    ) -> Result<()> {
        let addr = format!("0.0.0.0:{}", port);
        let listener = TcpListener::bind(&addr)
            .map_err(|e| MistError::Io(format!("Failed to bind pairing server on {}: {}", addr, e)))?;
        listener.set_nonblocking(true).map_err(|e| MistError::Io(e.to_string()))?;

        *self.is_running.lock().unwrap() = true;
        let is_running = self.is_running.clone();

        std::thread::spawn(move || {
            tracing::info!("Remote pairing server listening on {}", addr);
            while *is_running.lock().unwrap() {
                match listener.accept() {
                    Ok((mut stream, _client_addr)) => {
                        let mgr = manager.clone();
                        let cb = approval_cb.clone();
                        let decisions = decision_receiver.clone();

                        std::thread::spawn(move || {
                            let _ = stream.set_read_timeout(Some(Duration::from_secs(45)));
                            let _ = stream.set_write_timeout(Some(Duration::from_secs(10)));

                            let mut len_buf = [0u8; 4];
                            if stream.read_exact(&mut len_buf).is_err() {
                                return;
                            }
                            let len = u32::from_be_bytes(len_buf) as usize;
                            if len > 65536 {
                                return;
                            }
                            let mut payload = vec![0u8; len];
                            if stream.read_exact(&mut payload).is_err() {
                                return;
                            }

                            let req: RemotePairingRequest = match serde_json::from_slice(&payload) {
                                Ok(r) => r,
                                Err(e) => {
                                    tracing::error!("Pairing request decode error: {}", e);
                                    return;
                                }
                            };

                            let req_perms = DevicePermissions::from_mask(req.requested_permissions);
                            let sub_res = {
                                let mut m = mgr.lock().unwrap();
                                m.submit_pairing_request(&req)
                            };

                            let response = match sub_res {
                                Ok(req_id) => {
                                    // Register pending decision slot
                                    decisions.lock().unwrap().insert(req_id, None);
                                    // Notify Host UI callback
                                    cb(
                                        req_id,
                                        req.client_device_id.clone(),
                                        req.client_device_name.clone(),
                                        req_perms,
                                    );

                                    // Wait for host user decision (up to 40 seconds)
                                    let deadline = Instant::now() + Duration::from_secs(40);
                                    let mut final_resp = None;

                                    while Instant::now() < deadline {
                                        std::thread::sleep(Duration::from_millis(100));
                                        let mut dec = decisions.lock().unwrap();
                                        if let Some(Some(resp)) = dec.remove(&req_id) {
                                            final_resp = Some(resp);
                                            break;
                                        }
                                    }

                                    final_resp.unwrap_or_else(|| RemotePairingResponse {
                                        success: false,
                                        status: "timeout".to_string(),
                                        auth_token: String::new(),
                                        session_id: String::new(),
                                        approved_permissions: 0,
                                        error_message:
                                            "Pairing request timed out awaiting host approval."
                                                .to_string(),
                                    })
                                }
                                Err(e) => RemotePairingResponse {
                                    success: false,
                                    status: "rejected".to_string(),
                                    auth_token: String::new(),
                                    session_id: String::new(),
                                    approved_permissions: 0,
                                    error_message: e.to_string(),
                                },
                            };

                            if let Ok(resp_bytes) = serde_json::to_vec(&response) {
                                let resp_len = (resp_bytes.len() as u32).to_be_bytes();
                                let _ = stream.write_all(&resp_len);
                                let _ = stream.write_all(&resp_bytes);
                                let _ = stream.flush();
                            }
                        });
                    }
                    Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                        std::thread::sleep(Duration::from_millis(50));
                    }
                    Err(e) => {
                        tracing::warn!("Pairing server accept error: {}", e);
                        std::thread::sleep(Duration::from_millis(100));
                    }
                }
            }
        });

        Ok(())
    }

    pub fn stop(&mut self) {
        *self.is_running.lock().unwrap() = false;
    }
}

pub struct RemotePairingClient;

impl RemotePairingClient {
    pub fn submit_request(
        host: &str,
        port: u16,
        request: RemotePairingRequest,
        timeout: Duration,
    ) -> Result<RemotePairingResponse> {
        let addr = format!("{}:{}", host, port);
        let stream = TcpStream::connect(&addr)
            .map_err(|e| MistError::Pairing(format!("Failed to connect to host at {}: {}", addr, e)))?;

        stream
            .set_read_timeout(Some(timeout))
            .map_err(|e| MistError::Io(e.to_string()))?;
        stream
            .set_write_timeout(Some(Duration::from_secs(5)))
            .map_err(|e| MistError::Io(e.to_string()))?;

        let mut stream = stream;
        let mut req = request;
        if req.nonce.is_empty() {
            let nonce_bytes = crypto::secure_random_bytes(16);
            req.nonce = crypto::hex_encode(&nonce_bytes);
        }
        if req.timestamp == 0 {
            req.timestamp = Utc::now().timestamp() as u64;
        }

        let payload = serde_json::to_vec(&req)
            .map_err(|e| MistError::Pairing(format!("Failed to encode request: {}", e)))?;

        let len_buf = (payload.len() as u32).to_be_bytes();
        stream.write_all(&len_buf).map_err(|e| MistError::Io(e.to_string()))?;
        stream.write_all(&payload).map_err(|e| MistError::Io(e.to_string()))?;
        stream.flush().map_err(|e| MistError::Io(e.to_string()))?;

        let mut resp_len_buf = [0u8; 4];
        stream.read_exact(&mut resp_len_buf).map_err(|e| {
            MistError::Pairing(format!(
                "No response from host (timeout or connection closed): {}",
                e
            ))
        })?;

        let resp_len = u32::from_be_bytes(resp_len_buf) as usize;
        if resp_len > 65536 {
            return Err(MistError::Pairing("Response exceeds maximum size limit".to_string()));
        }

        let mut resp_payload = vec![0u8; resp_len];
        stream.read_exact(&mut resp_payload).map_err(|e| MistError::Io(e.to_string()))?;

        let resp: RemotePairingResponse = serde_json::from_slice(&resp_payload)
            .map_err(|e| MistError::Pairing(format!("Failed to decode host response: {}", e)))?;

        Ok(resp)
    }
}
