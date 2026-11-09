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
}

impl Default for DevicePermissions {
    fn default() -> Self {
        Self {
            keyboard: false,
            mouse: false,
            gamepad: false,
            audio: true,
        }
    }
}

impl DevicePermissions {
    pub fn all() -> Self {
        Self {
            keyboard: true,
            mouse: true,
            gamepad: true,
            audio: true,
        }
    }

    pub fn to_mask(&self) -> u32 {
        let mut mask = 0u32;
        if self.keyboard { mask |= 0x01; }
        if self.mouse { mask |= 0x02; }
        if self.gamepad { mask |= 0x04; }
        if self.audio { mask |= 0x08; }
        mask
    }

    pub fn from_mask(mask: u32) -> Self {
        Self {
            keyboard: (mask & 0x01) != 0,
            mouse: (mask & 0x02) != 0,
            gamepad: (mask & 0x04) != 0,
            audio: (mask & 0x08) != 0,
        }
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
