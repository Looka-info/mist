//! Typed configuration and persistence for Mist.

use mist_core::{MistError, Result};
use mist_domain::QualityProfile;
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

pub const MIN_BITRATE_KBPS: u32 = 2_000;
pub const MAX_BITRATE_KBPS: u32 = 25_000;
pub const DEFAULT_BITRATE_KBPS: u32 = 12_000;
pub const DEFAULT_FPS: u32 = 60;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct VideoConfig {
    pub width: u32,
    pub height: u32,
    pub fps: u32,
    pub bitrate_kbps: u32,
    pub profile: QualityProfile,
    pub vsync: bool,
    #[serde(default = "default_codec")]
    pub codec: String,
    #[serde(default)]
    pub hdr_enabled: bool,
    #[serde(default = "default_backend")]
    pub capture_backend: String,
    #[serde(default)]
    pub display_index: u32,
    #[serde(default = "default_true")]
    pub relative_mouse: bool,
    #[serde(default)]
    pub wol_mac_address: String,
    #[serde(default = "default_virtual_display_res")]
    pub virtual_display_resolution: String,
    #[serde(default)]
    pub virtual_display_enabled: bool,
}

fn default_codec() -> String { "auto".to_string() }
fn default_backend() -> String { "auto".to_string() }
fn default_true() -> bool { true }
fn default_virtual_display_res() -> String { "1920x1080@60".to_string() }

impl Default for VideoConfig {
    fn default() -> Self {
        Self {
            width: 1920,
            height: 1080,
            fps: DEFAULT_FPS,
            bitrate_kbps: DEFAULT_BITRATE_KBPS,
            profile: QualityProfile::Balanced,
            vsync: true,
            codec: default_codec(),
            hdr_enabled: false,
            capture_backend: default_backend(),
            display_index: 0,
            relative_mouse: true,
            wol_mac_address: String::new(),
            virtual_display_resolution: default_virtual_display_res(),
            virtual_display_enabled: false,
        }
    }
}

impl VideoConfig {
    pub fn validate(&mut self) {
        if self.bitrate_kbps < MIN_BITRATE_KBPS {
            self.bitrate_kbps = MIN_BITRATE_KBPS;
        } else if self.bitrate_kbps > MAX_BITRATE_KBPS {
            self.bitrate_kbps = MAX_BITRATE_KBPS;
        }
        if self.fps == 0 {
            self.fps = 60;
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioConfig {
    pub enabled: bool,
    pub sample_rate: u32,
    pub channels: u32,
}

impl Default for AudioConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            sample_rate: 48000,
            channels: 2,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NetworkConfig {
    pub signaling_url: String,
    pub stun_server: String,
    pub stun_servers: Vec<String>,
    pub turn_server: String,
    pub turn_username: String,
    pub turn_password: String,
    pub pairing_port: u16,
    pub enable_upnp: bool,
    pub enable_nat_pmp: bool,
    pub port_mapping_lease_seconds: u32,
    pub enable_ipv6: bool,
    pub ice_min_port: u16,
    pub ice_max_port: u16,
    #[serde(default = "default_max_upload_limit_kbps")]
    pub max_upload_limit_kbps: u32,
}

fn default_max_upload_limit_kbps() -> u32 {
    30_000
}

impl Default for NetworkConfig {
    fn default() -> Self {
        Self {
            signaling_url: "ws://127.0.0.1:8080".to_string(),
            stun_server: "stun:stun.l.google.com:19302".to_string(),
            stun_servers: vec![
                "stun:stun.l.google.com:19302".to_string(),
                "stun:stun1.l.google.com:19302".to_string(),
            ],
            turn_server: String::new(),
            turn_username: String::new(),
            turn_password: String::new(),
            pairing_port: 8085,
            enable_upnp: false,
            enable_nat_pmp: false,
            port_mapping_lease_seconds: 3600,
            enable_ipv6: true,
            ice_min_port: 0,
            ice_max_port: 0,
            max_upload_limit_kbps: 30_000,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UiConfig {
    pub dark_theme: bool,
    pub high_contrast: bool,
    pub reduced_motion: bool,
}

impl Default for UiConfig {
    fn default() -> Self {
        Self {
            dark_theme: true,
            high_contrast: false,
            reduced_motion: false,
        }
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct AppConfig {
    pub video: VideoConfig,
    pub audio: AudioConfig,
    pub network: NetworkConfig,
    pub ui: UiConfig,
}

impl AppConfig {
    pub fn config_path() -> PathBuf {
        if let Ok(appdata) = std::env::var("APPDATA") {
            let p = Path::new(&appdata).join("Mist");
            let _ = fs::create_dir_all(&p);
            p.join("config.json")
        } else {
            PathBuf::from("mist_config.json")
        }
    }

    pub fn load() -> Self {
        let p = Self::config_path();
        if p.exists() {
            if let Ok(content) = fs::read_to_string(&p) {
                if let Ok(mut cfg) = serde_json::from_str::<AppConfig>(&content) {
                    cfg.video.validate();
                    return cfg;
                }
            }
        }
        let mut def = AppConfig::default();
        def.video.validate();
        def
    }

    pub fn save(&self) -> Result<()> {
        let p = Self::config_path();
        let content = serde_json::to_string_pretty(self)
            .map_err(|e| MistError::Config(format!("Serialization error: {}", e)))?;
        fs::write(&p, content)
            .map_err(|e| MistError::Io(format!("Failed to write config file {}: {}", p.display(), e)))?;
        Ok(())
    }
}
