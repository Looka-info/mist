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

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum RouteType {
    DirectHost,
    DirectSrflx,
    Relayed,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum NatType {
    Unknown,
    OpenInternetDirect,
    FullCone,
    AddressRestrictedCone,
    PortRestrictedCone,
    Symmetric,
    CarrierGradeNat,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NatDiagnostics {
    pub nat_type: NatType,
    pub route_type: RouteType,
    pub is_behind_cgnat: bool,
    pub is_behind_symmetric_nat: bool,
    pub host_candidates_count: u32,
    pub srflx_candidates_count: u32,
    pub relay_candidates_count: u32,
    pub ipv4_candidates_count: u32,
    pub ipv6_candidates_count: u32,
    pub selected_pair: String,
    pub upnp_active: bool,
    pub failure_reason: Option<String>,
}

impl Default for NatDiagnostics {
    fn default() -> Self {
        Self {
            nat_type: NatType::Unknown,
            route_type: RouteType::Unknown,
            is_behind_cgnat: false,
            is_behind_symmetric_nat: false,
            host_candidates_count: 0,
            srflx_candidates_count: 0,
            relay_candidates_count: 0,
            ipv4_candidates_count: 0,
            ipv6_candidates_count: 0,
            selected_pair: "none".to_string(),
            upnp_active: false,
            failure_reason: None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GuestTelemetrySummary {
    pub guest_id: String,
    pub client_name: String,
    pub rtt_ms: f32,
    pub bitrate_kbps: u32,
    pub loss_percent: f32,
    pub quality_tier: String,
    pub is_active_player: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StreamOverlayStats {
    pub is_visible: bool,
    pub is_detailed: bool,
    pub fps: f32,
    pub bitrate_kbps: u32,
    pub rtt_ms: f32,
    pub jitter_ms: f32,
    pub packet_loss_percent: f32,
    pub capture_latency_us: f32,
    pub encode_latency_us: f32,
    pub decode_latency_us: f32,
    pub present_latency_us: f32,
    pub total_pipeline_latency_ms: f32,
    pub video_codec: String,
    pub hardware_backend: String,
    pub resolution: String,
    pub fec_recovered_packets: u64,
    pub audio_plc_count: u64,
    pub is_host: bool,
    pub connected_guests: Vec<GuestTelemetrySummary>,
}

impl Default for StreamOverlayStats {
    fn default() -> Self {
        Self {
            is_visible: true,
            is_detailed: false,
            fps: 60.0,
            bitrate_kbps: 15000,
            rtt_ms: 8.5,
            jitter_ms: 1.0,
            packet_loss_percent: 0.0,
            capture_latency_us: 850.0,
            encode_latency_us: 1200.0,
            decode_latency_us: 1800.0,
            present_latency_us: 500.0,
            total_pipeline_latency_ms: 12.5,
            video_codec: "HEVC / H.265".to_string(),
            hardware_backend: "NVIDIA NVENC → D3D11VA".to_string(),
            resolution: "1920 × 1080 @ 60 Hz".to_string(),
            fec_recovered_packets: 0,
            audio_plc_count: 0,
            is_host: false,
            connected_guests: Vec::new(),
        }
    }
}

impl StreamOverlayStats {
    pub fn formatted_fps(&self) -> String {
        format!("{:.0} FPS", self.fps)
    }

    pub fn formatted_bitrate(&self) -> String {
        format!("{:.1} Mbps", self.bitrate_kbps as f32 / 1000.0)
    }

    pub fn formatted_rtt(&self) -> String {
        format!("{:.1} ms", self.rtt_ms)
    }

    pub fn formatted_jitter(&self) -> String {
        format!("{:.1} ms", self.jitter_ms)
    }

    pub fn formatted_loss(&self) -> String {
        format!("{:.1}%", self.packet_loss_percent)
    }

    pub fn formatted_total_latency(&self) -> String {
        format!("{:.1} ms", self.total_pipeline_latency_ms)
    }

    pub fn health_status(&self) -> &'static str {
        if self.rtt_ms > 80.0 || self.packet_loss_percent > 3.0 {
            "Poor"
        } else if self.rtt_ms > 40.0 || self.packet_loss_percent > 1.0 {
            "Fair"
        } else {
            "Optimal"
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_stream_overlay_stats_defaults_and_formatting() {
        let stats = StreamOverlayStats::default();
        assert!(stats.is_visible);
        assert_eq!(stats.formatted_fps(), "60 FPS");
        assert_eq!(stats.formatted_bitrate(), "15.0 Mbps");
        assert_eq!(stats.formatted_rtt(), "8.5 ms");
        assert_eq!(stats.formatted_loss(), "0.0%");
        assert_eq!(stats.formatted_total_latency(), "12.5 ms");
        assert_eq!(stats.health_status(), "Optimal");
    }

    #[test]
    fn test_stream_overlay_health_degradation() {
        let mut stats = StreamOverlayStats {
            rtt_ms: 45.0,
            ..Default::default()
        };
        assert_eq!(stats.health_status(), "Fair");

        stats.packet_loss_percent = 5.0;
        assert_eq!(stats.health_status(), "Poor");
    }

    #[test]
    fn test_guest_telemetry_serialization() {
        let guest = GuestTelemetrySummary {
            guest_id: "guest-1".to_string(),
            client_name: "SteamDeck".to_string(),
            rtt_ms: 12.4,
            bitrate_kbps: 10000,
            loss_percent: 0.1,
            quality_tier: "Quality".to_string(),
            is_active_player: true,
        };

        let json = serde_json::to_string(&guest).expect("Serialization failed");
        assert!(json.contains("SteamDeck"));
        assert!(json.contains("Quality"));
    }
}
