//! Domain types, identifiers, permissions, and state models for Mist.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct DeviceId(pub String);

impl DeviceId {
    pub fn new(id: impl Into<String>) -> Self {
        Self(id.into())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct SessionId(pub String);

impl SessionId {
    pub fn new(id: impl Into<String>) -> Self {
        Self(id.into())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct DevicePermissions {
    pub keyboard: bool,
    pub mouse: bool,
    pub gamepad: bool,
    pub audio: bool,
    #[serde(default)]
    pub clipboard: bool,
}

impl Default for DevicePermissions {
    fn default() -> Self {
        Self {
            keyboard: false,
            mouse: false,
            gamepad: false,
            audio: true,
            clipboard: false,
        }
    }
}

impl DevicePermissions {
    pub fn view_only() -> Self {
        Self {
            keyboard: false,
            mouse: false,
            gamepad: false,
            audio: true,
            clipboard: false,
        }
    }

    pub fn keyboard_and_mouse() -> Self {
        Self {
            keyboard: true,
            mouse: true,
            gamepad: false,
            audio: true,
            clipboard: false,
        }
    }

    pub fn gamepad_only() -> Self {
        Self {
            keyboard: false,
            mouse: false,
            gamepad: true,
            audio: true,
            clipboard: false,
        }
    }

    pub fn all() -> Self {
        Self {
            keyboard: true,
            mouse: true,
            gamepad: true,
            audio: true,
            clipboard: true,
        }
    }

    pub fn to_mask(&self) -> u32 {
        let mut mask = 0u32;
        if self.keyboard {
            mask |= 0x01;
        }
        if self.mouse {
            mask |= 0x02;
        }
        if self.gamepad {
            mask |= 0x04;
        }
        if self.audio {
            mask |= 0x08;
        }
        if self.clipboard {
            mask |= 0x10;
        }
        mask
    }

    pub fn from_mask(mask: u32) -> Self {
        Self {
            keyboard: (mask & 0x01) != 0,
            mouse: (mask & 0x02) != 0,
            gamepad: (mask & 0x04) != 0,
            audio: (mask & 0x08) != 0,
            clipboard: (mask & 0x10) != 0,
        }
    }

    pub fn is_view_only(&self) -> bool {
        !self.keyboard && !self.mouse && !self.gamepad && !self.clipboard
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct DeviceEntry {
    pub id: DeviceId,
    pub name: String,
    pub last_connected: String,
    pub permissions: DevicePermissions,
    pub is_trusted: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GuestIdentityRecord {
    pub device_id: DeviceId,
    pub device_name: String,
    pub first_seen: String,
    pub last_authenticated: String,
    pub connection_count: u64,
    pub permissions: DevicePermissions,
    pub is_trusted: bool,
    pub auth_token_hash: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct AuthorizedSessionTicket {
    pub auth_token_hash: String,
    pub session_id: String,
    pub client_device_id: String,
    pub client_device_name: String,
    pub issued_at: String,
    pub expires_at: String,
    pub permissions: DevicePermissions,
    pub consumed: bool,
    #[serde(default)]
    pub client_fingerprint: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct BannedDeviceRecord {
    pub device_id: DeviceId,
    pub device_name: String,
    pub banned_at: String,
    pub reason: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ConnectedGuest {
    pub device_id: DeviceId,
    pub device_name: String,
    pub session_id: String,
    pub authenticated_at: String,
    pub permissions: DevicePermissions,
    pub token_hash: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PermissionAuditEvent {
    pub timestamp: String,
    pub device_id: DeviceId,
    pub action: String,
    pub allowed: bool,
    pub detail: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ConnectionState {
    Idle,
    Initializing,
    Pairing,
    AwaitingApproval,
    Connecting,
    Streaming,
    Reconnecting,
    Disconnected,
    Error,
}

impl ConnectionState {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Idle => "Idle",
            Self::Initializing => "Initializing",
            Self::Pairing => "Pairing",
            Self::AwaitingApproval => "Awaiting Approval",
            Self::Connecting => "Connecting",
            Self::Streaming => "Streaming",
            Self::Reconnecting => "Reconnecting",
            Self::Disconnected => "Disconnected",
            Self::Error => "Error",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum QualityProfile {
    Performance,
    Balanced,
    Quality,
    Adaptive,
}

impl QualityProfile {
    pub fn default_bitrate_kbps(&self) -> u32 {
        match self {
            Self::Performance => 6_000,
            Self::Balanced => 12_000,
            Self::Quality => 20_000,
            Self::Adaptive => 10_000,
        }
    }

    pub fn default_fps(&self) -> u32 {
        match self {
            Self::Performance => 60,
            Self::Balanced => 60,
            Self::Quality => 60,
            Self::Adaptive => 60,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct GuestQualityStatus {
    pub guest_id: String,
    pub profile: QualityProfile,
    pub target_bitrate_kbps: u32,
    pub width: u32,
    pub height: u32,
    pub fps: u32,
    pub rendition_index: u32,
    pub rtt_ms: f64,
    pub loss_percent: f64,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct HostBandwidthBudget {
    pub upload_limit_kbps: u32,
    pub total_allocated_kbps: u32,
    pub video_allocated_kbps: u32,
    pub audio_reserved_kbps: u32,
    pub control_reserved_kbps: u32,
    pub headroom_kbps: u32,
    pub unallocated_kbps: u32,
    pub is_oversubscribed: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SecurityTrustState {
    Unverified,
    Verified,
    FingerprintMismatch,
    IdentityChanged,
}

impl SecurityTrustState {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Unverified => "Unverified",
            Self::Verified => "Verified",
            Self::FingerprintMismatch => "Fingerprint Mismatch",
            Self::IdentityChanged => "Identity Changed",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SecurityStatus {
    pub is_encrypted: bool,
    pub cipher_suite: String,
    pub peer_fingerprint: String,
    pub sas_code: String,
    pub trust_state: SecurityTrustState,
    pub status_message: String,
}

impl Default for SecurityStatus {
    fn default() -> Self {
        Self {
            is_encrypted: false,
            cipher_suite: String::new(),
            peer_fingerprint: String::new(),
            sas_code: String::new(),
            trust_state: SecurityTrustState::Unverified,
            status_message: "No active secure transport session".to_string(),
        }
    }
}
