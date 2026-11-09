//! Application controller and state coordination for Mist Desktop.

use mist_config::AppConfig;
use mist_core::Result;
use mist_domain::{DeviceEntry, DevicePermissions};
use mist_pairing::{
    PairingManager, RemotePairingResponse, RemotePairingServer,
};
use mist_platform_windows::{get_clipboard_text, query_hardware_info, query_wifi_info, set_clipboard_text, HardwareInfo, WifiInfo};
use mist_session::{SessionEvent, SessionSupervisor};
use std::collections::HashMap;
use std::sync::{Arc, Mutex};

pub struct DesktopController {
    pub config: Arc<Mutex<AppConfig>>,
    pub pairing_manager: Arc<Mutex<PairingManager>>,
    pub session_supervisor: Arc<Mutex<SessionSupervisor>>,
    pub pairing_server: Arc<Mutex<RemotePairingServer>>,
    pub wifi_info: Arc<Mutex<WifiInfo>>,
    pub hardware_info: Arc<Mutex<HardwareInfo>>,
    pub pending_decisions: Arc<Mutex<HashMap<u64, Option<RemotePairingResponse>>>>,
    pub current_pending_req_id: Arc<Mutex<Option<u64>>>,
}

impl Default for DesktopController {
    fn default() -> Self {
        let config = AppConfig::load();
        let pairing_manager = PairingManager::new();
        let session_supervisor = SessionSupervisor::new();
        let pairing_server = RemotePairingServer::new();
        let wifi_info = query_wifi_info();
        let hardware_info = query_hardware_info();

        Self {
            config: Arc::new(Mutex::new(config)),
            pairing_manager: Arc::new(Mutex::new(pairing_manager)),
            session_supervisor: Arc::new(Mutex::new(session_supervisor)),
            pairing_server: Arc::new(Mutex::new(pairing_server)),
            wifi_info: Arc::new(Mutex::new(wifi_info)),
            hardware_info: Arc::new(Mutex::new(hardware_info)),
            pending_decisions: Arc::new(Mutex::new(HashMap::new())),
            current_pending_req_id: Arc::new(Mutex::new(None)),
        }
    }
}

impl DesktopController {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn refresh_platform_info(&self) {
        *self.wifi_info.lock().unwrap() = query_wifi_info();
        *self.hardware_info.lock().unwrap() = query_hardware_info();
    }

    pub fn start_sharing<F>(&self, event_cb: F, approval_prompt_cb: Arc<dyn Fn(u64, String, String) + Send + Sync + 'static>) -> Result<String>
    where
        F: Fn(SessionEvent) + Send + Sync + 'static,
    {
        let session_id = format!("mist-sess-{}", std::process::id());
        let code = {
            let mut pm = self.pairing_manager.lock().unwrap();
            pm.generate_pairing_code(session_id.clone())
        };

        // Start Remote Pairing Server on port 8085
        let pm_arc = self.pairing_manager.clone();
        let decisions_arc = self.pending_decisions.clone();
        let prompt_cb = approval_prompt_cb.clone();
        let current_req_arc = self.current_pending_req_id.clone();

        let mut server = self.pairing_server.lock().unwrap();
        server.start(
            8085,
            pm_arc,
            Arc::new(move |req_id, client_id, client_name, _perms| {
                *current_req_arc.lock().unwrap() = Some(req_id);
                prompt_cb(req_id, client_id, client_name);
            }),
            decisions_arc,
        )?;

        // Launch Host Streaming Process
        let cfg = self.config.lock().unwrap().clone();
        let mut sup = self.session_supervisor.lock().unwrap();
        sup.start_host(&session_id, &cfg, event_cb)?;

        Ok(code)
    }

    pub fn stop_sharing(&self) {
        self.session_supervisor.lock().unwrap().stop();
        self.pairing_server.lock().unwrap().stop();
        self.pairing_manager.lock().unwrap().revoke_code();
    }

    pub fn approve_pending_request(&self) -> bool {
        let req_id = self.current_pending_req_id.lock().unwrap().take();
        if let Some(id) = req_id {
            let (auth_token, _device_name) = {
                let mut pm = self.pairing_manager.lock().unwrap();
                pm.approve_request(id, DevicePermissions::all())
                    .unwrap_or((String::new(), String::new()))
            };

            let resp = RemotePairingResponse {
                success: true,
                status: "approved".to_string(),
                auth_token,
                session_id: format!("mist-sess-{}", std::process::id()),
                approved_permissions: 0x0F,
                error_message: String::new(),
                host_fingerprint: String::new(),
                sas_code: String::new(),
            };

            self.pending_decisions.lock().unwrap().insert(id, Some(resp));
            true
        } else {
            false
        }
    }

    pub fn deny_pending_request(&self) -> bool {
        let req_id = self.current_pending_req_id.lock().unwrap().take();
        if let Some(id) = req_id {
            {
                let mut pm = self.pairing_manager.lock().unwrap();
                pm.deny_request(id);
            }

            let resp = RemotePairingResponse {
                success: false,
                status: "rejected".to_string(),
                auth_token: String::new(),
                session_id: String::new(),
                approved_permissions: 0,
                error_message: "Host explicitly denied the connection request.".to_string(),
                host_fingerprint: String::new(),
                sas_code: String::new(),
            };

            self.pending_decisions.lock().unwrap().insert(id, Some(resp));
            true
        } else {
            false
        }
    }

    pub fn copy_active_code(&self) -> bool {
        if let Some(code) = self.pairing_manager.lock().unwrap().active_code() {
            set_clipboard_text(&code)
        } else {
            false
        }
    }

    pub fn paste_code_from_clipboard(&self) -> Option<String> {
        get_clipboard_text().map(|s| s.trim().to_uppercase())
    }

    pub fn revoke_trusted_device(&self, device_id: &str) -> bool {
        self.pairing_manager.lock().unwrap().revoke_device(device_id)
    }

    pub fn get_trusted_devices(&self) -> Vec<DeviceEntry> {
        self.pairing_manager.lock().unwrap().trusted_devices()
    }

    #[allow(dead_code)]
    pub fn kick_guest(&self, device_id: &str) -> bool {
        self.pairing_manager.lock().unwrap().kick_guest(device_id)
    }

    #[allow(dead_code)]
    pub fn ban_device(&self, device_id: &str, reason: Option<&str>) -> bool {
        self.pairing_manager
            .lock()
            .unwrap()
            .ban_device(device_id, None, reason.map(|s| s.to_string()))
    }

    #[allow(dead_code)]
    pub fn unban_device(&self, device_id: &str) -> bool {
        self.pairing_manager.lock().unwrap().unban_device(device_id)
    }

    #[allow(dead_code)]
    pub fn get_banned_devices(&self) -> Vec<mist_domain::BannedDeviceRecord> {
        self.pairing_manager.lock().unwrap().banned_devices()
    }

    #[allow(dead_code)]
    pub fn get_active_connected_guests(&self) -> Vec<mist_domain::ConnectedGuest> {
        self.pairing_manager.lock().unwrap().active_connected_guests()
    }

    #[allow(dead_code)]
    pub fn update_guest_permissions(&self, device_id: &str, perms: DevicePermissions) -> bool {
        self.pairing_manager.lock().unwrap().update_device_permissions(device_id, perms)
    }

    #[allow(dead_code)]
    pub fn revoke_guest_permissions(&self, device_id: &str) -> bool {
        self.pairing_manager.lock().unwrap().revoke_guest_permissions(device_id)
    }

    #[allow(dead_code)]
    pub fn get_permission_audit_log(&self) -> Vec<mist_domain::PermissionAuditEvent> {
        self.pairing_manager.lock().unwrap().permission_audit_log().to_vec()
    }

    pub fn send_wol(&self, mac_str: &str) -> Result<()> {
        mist_platform_windows::send_wake_on_lan(mac_str)
            .map_err(|e| mist_core::MistError::Internal(e))
    }

    pub fn cycle_display(&self) -> u32 {
        let mut cfg = self.config.lock().unwrap();
        cfg.video.display_index = (cfg.video.display_index + 1) % 4;
        let idx = cfg.video.display_index;
        let _ = cfg.save();
        idx
    }

    pub fn toggle_relative_mouse(&self) -> bool {
        let mut cfg = self.config.lock().unwrap();
        cfg.video.relative_mouse = !cfg.video.relative_mouse;
        let val = cfg.video.relative_mouse;
        let _ = cfg.save();
        val
    }

    pub fn toggle_virtual_display(&self) -> bool {
        let mut cfg = self.config.lock().unwrap();
        cfg.video.virtual_display_enabled = !cfg.video.virtual_display_enabled;
        let val = cfg.video.virtual_display_enabled;
        let _ = cfg.save();
        val
    }
}
