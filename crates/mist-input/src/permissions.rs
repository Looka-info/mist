//! Granular multi-guest input authorization and permission evaluation.

use crate::types::*;
use mist_domain::DevicePermissions;
use std::collections::HashMap;
use std::sync::RwLock;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum PermissionStatus {
    Disabled = 0,
    Requested = 1,
    Granted = 2,
    Revoked = 3,
}

pub struct InputPermissionManager {
    authorized_session_id: RwLock<String>,
    guest_permissions: RwLock<HashMap<String, DevicePermissions>>,
    local_override: RwLock<bool>,
}

impl Default for InputPermissionManager {
    fn default() -> Self {
        Self::new("")
    }
}

impl InputPermissionManager {
    pub const FEATURE_VIEW_ONLY: u8 = 0x00;
    pub const FEATURE_KEYBOARD: u8 = 0x01;
    pub const FEATURE_MOUSE: u8 = 0x02;
    pub const FEATURE_GAMEPAD: u8 = 0x04;
    pub const FEATURE_AUDIO: u8 = 0x08;
    pub const FEATURE_CLIPBOARD: u8 = 0x10;

    pub fn new(authorized_session_id: impl Into<String>) -> Self {
        Self {
            authorized_session_id: RwLock::new(authorized_session_id.into()),
            guest_permissions: RwLock::new(HashMap::new()),
            local_override: RwLock::new(false),
        }
    }

    pub fn set_authorized_session_id(&self, session_id: impl Into<String>) {
        *self.authorized_session_id.write().unwrap() = session_id.into();
    }

    pub fn authorized_session_id(&self) -> String {
        self.authorized_session_id.read().unwrap().clone()
    }

    pub fn set_guest_permissions(&self, guest_id: &str, perms: DevicePermissions) {
        self.guest_permissions
            .write()
            .unwrap()
            .insert(guest_id.to_string(), perms);
    }

    pub fn remove_guest(&self, guest_id: &str) {
        self.guest_permissions.write().unwrap().remove(guest_id);
    }

    pub fn set_local_override(&self, override_active: bool) {
        *self.local_override.write().unwrap() = override_active;
    }

    pub fn is_input_allowed(&self, input_type: InputType, session_id: &str) -> bool {
        if *self.local_override.read().unwrap() {
            return false;
        }

        // 1. Check guest permissions table first
        let perms_lock = self.guest_permissions.read().unwrap();
        if let Some(perms) = perms_lock.get(session_id) {
            return match input_type {
                InputType::KeyEvent => perms.keyboard,
                InputType::MouseMoveEvent
                | InputType::MouseButtonEvent
                | InputType::MouseWheelEvent => perms.mouse,
                InputType::GamepadState => perms.gamepad,
                InputType::ClipboardEvent => perms.clipboard,
                InputType::PermissionRequest | InputType::ResetState => true,
                _ => false,
            };
        }

        // 2. Fallback to authorized single-session check
        let auth_id = self.authorized_session_id.read().unwrap();
        if !auth_id.is_empty() && auth_id.as_str() == session_id {
            return true;
        }

        false
    }
}
