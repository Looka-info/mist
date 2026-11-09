//! Telemetry models, metrics collection, and real-time network diagnostics.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionTelemetry {
    pub fps: f32,
    pub bitrate_kbps: u32,
    pub rtt_ms: f32,
    pub jitter_ms: f32,
    pub packet_loss_percent: f32,
    pub frames_decoded: u32,
    pub frames_dropped: u32,
    pub timestamp_ns: u64,
    pub source: String,
}

impl Default for SessionTelemetry {
    fn default() -> Self {
        Self {
            fps: 0.0,
            bitrate_kbps: 0,
            rtt_ms: 0.0,
            jitter_ms: 0.0,
            packet_loss_percent: 0.0,
            frames_decoded: 0,
            frames_dropped: 0,
            timestamp_ns: 0,
            source: "none".to_string(),
        }
    }
}

impl SessionTelemetry {
    pub fn is_active(&self) -> bool {
        self.timestamp_ns > 0 && (self.fps > 0.0 || self.bitrate_kbps > 0)
    }

    pub fn formatted_bitrate(&self) -> String {
        if self.bitrate_kbps == 0 {
            "Unavailable".to_string()
        } else {
            format!("{:.1} Mbps", self.bitrate_kbps as f32 / 1000.0)
        }
    }

    pub fn formatted_fps(&self) -> String {
        if self.fps <= 0.0 {
            "Unavailable".to_string()
        } else {
            format!("{:.0} FPS", self.fps)
        }
    }

    pub fn formatted_rtt(&self) -> String {
        if self.rtt_ms <= 0.0 {
            "Unavailable".to_string()
        } else {
            format!("{:.1} ms", self.rtt_ms)
        }
    }

    pub fn formatted_jitter(&self) -> String {
        if self.jitter_ms <= 0.0 {
            "Unavailable".to_string()
        } else {
            format!("{:.1} ms", self.jitter_ms)
        }
    }

    pub fn formatted_loss(&self) -> String {
        format!("{:.1}%", self.packet_loss_percent)
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct NetworkDiagnostics {
    pub gateway_ping_ms: Option<f32>,
    pub internet_ping_ms: Option<f32>,
    pub media_rtt_ms: Option<f32>,
    pub last_checked_ns: u64,
}

impl NetworkDiagnostics {
    pub fn formatted_gateway_ping(&self) -> String {
        match self.gateway_ping_ms {
            Some(ms) => format!("{:.1} ms", ms),
            None => "Unavailable".to_string(),
        }
    }

    pub fn formatted_internet_ping(&self) -> String {
        match self.internet_ping_ms {
            Some(ms) => format!("{:.1} ms", ms),
            None => "Unavailable".to_string(),
        }
    }

    pub fn formatted_media_rtt(&self) -> String {
        match self.media_rtt_ms {
            Some(ms) => format!("{:.1} ms", ms),
            None => "Unavailable".to_string(),
        }
    }
}
