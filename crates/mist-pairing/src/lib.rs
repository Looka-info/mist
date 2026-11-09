//! Pairing logic, rate limiting, host authorization, and remote pairing service.

use chrono::Utc;
use mist_core::{MistError, Result};
use mist_domain::{DeviceId, DevicePermissions, DeviceEntry};
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

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RemotePairingRequest {
    pub client_device_id: String,
    pub client_device_name: String,
    pub pairing_code: String,
    pub requested_permissions: u32,
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
    pub requested_permissions: DevicePermissions,
    pub timestamp: Instant,
}

struct ActiveCode {
    code: String,
    created_at: Instant,
    pub session_id: String,
}

pub struct PairingManager {
    active_code: Option<ActiveCode>,
    failed_attempts: u32,
    lockout_until: Option<Instant>,
    trusted_devices: HashMap<String, DeviceEntry>,
    pending_requests: Vec<PendingRequest>,
    next_request_id: u64,
}

impl Default for PairingManager {
    fn default() -> Self {
        let mut mgr = Self {
            active_code: None,
            failed_attempts: 0,
            lockout_until: None,
            trusted_devices: HashMap::new(),
            pending_requests: Vec::new(),
            next_request_id: 1,
        };
        mgr.load_trusted_devices();
        mgr
    }
}

impl PairingManager {
    pub fn new() -> Self {
        Self::default()
    }

    fn trusted_devices_path() -> PathBuf {
        if let Ok(appdata) = std::env::var("APPDATA") {
            let p = Path::new(&appdata).join("Mist");
            let _ = fs::create_dir_all(&p);
            p.join("trusted_devices.json")
        } else {
            PathBuf::from("mist_trusted_devices.json")
        }
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

    /// Validates code format and content. If valid, adds to pending requests queue.
    pub fn submit_request(
        &mut self,
        device_id: String,
        device_name: String,
        code: &str,
        requested_permissions: DevicePermissions,
    ) -> Result<u64> {
        if self.is_locked_out() {
            let remaining = self.lockout_remaining_secs();
            return Err(MistError::Pairing(format!(
                "Too many failed attempts. Locked out for {} seconds.",
                remaining
            )));
        }

        // Clean up expired code
        if let Some(ref c) = self.active_code {
            if c.created_at.elapsed().as_secs() >= PAIRING_CODE_TTL_SECS {
                self.active_code = None;
            }
        }

        let is_match = match self.active_code {
            Some(ref c) => c.code.eq_ignore_ascii_case(code),
            None => false,
        };

        if !is_match {
            self.failed_attempts += 1;
            if self.failed_attempts >= MAX_FAILED_ATTEMPTS {
                self.lockout_until = Some(Instant::now() + Duration::from_secs(LOCKOUT_DURATION_SECS));
                tracing::warn!("Pairing rate limit reached. Locked out for 60 seconds.");
                return Err(MistError::Pairing(
                    "Too many failed attempts. Locked out for 60 seconds.".to_string(),
                ));
            }
            return Err(MistError::Pairing(format!(
                "Invalid pairing code. {} attempt(s) remaining.",
                MAX_FAILED_ATTEMPTS - self.failed_attempts
            )));
        }

        // Code is valid! Invalidate single-use code
        self.active_code = None;
        self.failed_attempts = 0;

        let req_id = self.next_request_id;
        self.next_request_id += 1;

        self.pending_requests.push(PendingRequest {
            request_id: req_id,
            client_device_id: device_id,
            client_device_name: device_name,
            requested_permissions,
            timestamp: Instant::now(),
        });

        Ok(req_id)
    }

    pub fn pending_requests(&self) -> &[PendingRequest] {
        &self.pending_requests
    }

    pub fn approve_request(
        &mut self,
        request_id: u64,
        approved_permissions: DevicePermissions,
    ) -> Option<(String, String)> {
        if let Some(idx) = self.pending_requests.iter().position(|r| r.request_id == request_id) {
            let req = self.pending_requests.remove(idx);
            let auth_token = format!("auth-tok-{}-{}", req.client_device_id, Utc::now().timestamp());

            let entry = DeviceEntry {
                id: DeviceId::new(&req.client_device_id),
                name: req.client_device_name.clone(),
                last_connected: Utc::now().to_rfc3339(),
                permissions: approved_permissions,
                is_trusted: true,
            };
            self.trusted_devices.insert(req.client_device_id, entry);
            self.save_trusted_devices();

            Some((auth_token, req.client_device_name))
        } else {
            None
        }
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
        if res {
            self.save_trusted_devices();
        }
        res
    }

    pub fn update_device_permissions(&mut self, device_id: &str, perms: DevicePermissions) -> bool {
        if let Some(dev) = self.trusted_devices.get_mut(device_id) {
            dev.permissions = perms;
            self.save_trusted_devices();
            true
        } else {
            false
        }
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
                                m.submit_request(
                                    req.client_device_id.clone(),
                                    req.client_device_name.clone(),
                                    &req.pairing_code,
                                    req_perms.clone(),
                                )
                            };

                            let response = match sub_res {
                                Ok(req_id) => {
                                    // Register pending decision slot
                                    decisions.lock().unwrap().insert(req_id, None);
                                    // Notify Host UI callback
                                    cb(req_id, req.client_device_id.clone(), req.client_device_name.clone(), req_perms);

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
                                        error_message: "Pairing request timed out awaiting host approval.".to_string(),
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

        stream.set_read_timeout(Some(timeout)).map_err(|e| MistError::Io(e.to_string()))?;
        stream.set_write_timeout(Some(Duration::from_secs(5))).map_err(|e| MistError::Io(e.to_string()))?;

        let mut stream = stream;
        let payload = serde_json::to_vec(&request)
            .map_err(|e| MistError::Pairing(format!("Failed to encode request: {}", e)))?;

        let len_buf = (payload.len() as u32).to_be_bytes();
        stream.write_all(&len_buf).map_err(|e| MistError::Io(e.to_string()))?;
        stream.write_all(&payload).map_err(|e| MistError::Io(e.to_string()))?;
        stream.flush().map_err(|e| MistError::Io(e.to_string()))?;

        let mut resp_len_buf = [0u8; 4];
        stream.read_exact(&mut resp_len_buf).map_err(|e| {
            MistError::Pairing(format!("No response from host (timeout or connection closed): {}", e))
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
