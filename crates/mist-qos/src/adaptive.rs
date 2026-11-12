//! Quality of Service (QoS) and Adaptive Bitrate (ABR) controllers.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum QualityProfile {
    UltraLowLatency,
    Balanced,
    HighQuality,
    Adaptive,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CongestionState {
    Optimal,
    MildDelay,
    Congested,
    CriticallyCongested,
}

#[derive(Debug, Clone)]
pub struct NetworkQualityEstimator {
    smoothed_rtt_ms: f64,
    smoothed_loss_percent: f64,
    smoothed_jitter_ms: f64,
    estimated_bandwidth_bps: u32,
}

impl Default for NetworkQualityEstimator {
    fn default() -> Self {
        Self {
            smoothed_rtt_ms: 15.0,
            smoothed_loss_percent: 0.0,
            smoothed_jitter_ms: 2.0,
            estimated_bandwidth_bps: 50_000_000, // 50 Mbps default
        }
    }
}

impl NetworkQualityEstimator {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn update_rtt(&mut self, rtt_ms: f64) {
        // Exponential moving average (alpha = 0.2)
        self.smoothed_rtt_ms = 0.8 * self.smoothed_rtt_ms + 0.2 * rtt_ms.max(0.0);
    }

    pub fn update_loss(&mut self, loss_percent: f64) {
        self.smoothed_loss_percent =
            0.8 * self.smoothed_loss_percent + 0.2 * loss_percent.clamp(0.0, 100.0);
    }

    pub fn update_jitter(&mut self, jitter_ms: f64) {
        self.smoothed_jitter_ms = 0.8 * self.smoothed_jitter_ms + 0.2 * jitter_ms.max(0.0);
    }

    pub fn update_bandwidth(&mut self, bandwidth_bps: u32) {
        self.estimated_bandwidth_bps = bandwidth_bps;
    }

    pub fn smoothed_rtt_ms(&self) -> f64 {
        self.smoothed_rtt_ms
    }

    pub fn smoothed_loss_percent(&self) -> f64 {
        self.smoothed_loss_percent
    }

    pub fn smoothed_jitter_ms(&self) -> f64 {
        self.smoothed_jitter_ms
    }

    pub fn quality_score(&self) -> u8 {
        if self.smoothed_loss_percent > 8.0 || self.smoothed_rtt_ms > 120.0 {
            1
        } else if self.smoothed_loss_percent > 4.0 || self.smoothed_rtt_ms > 70.0 {
            2
        } else if self.smoothed_loss_percent > 1.5 || self.smoothed_rtt_ms > 45.0 {
            3
        } else if self.smoothed_loss_percent > 0.5 || self.smoothed_rtt_ms > 25.0 {
            4
        } else {
            5
        }
    }
}

pub struct AdaptiveBitrateController {
    profile: QualityProfile,
    min_bitrate_kbps: u32,
    max_bitrate_kbps: u32,
    current_bitrate_kbps: u32,
    estimator: NetworkQualityEstimator,
    last_tick_ms: u64,
}

impl Default for AdaptiveBitrateController {
    fn default() -> Self {
        Self::new(QualityProfile::Adaptive)
    }
}

impl AdaptiveBitrateController {
    pub fn new(profile: QualityProfile) -> Self {
        let (min, max, initial) = match profile {
            QualityProfile::UltraLowLatency => (2_000, 20_000, 10_000),
            QualityProfile::Balanced => (3_000, 35_000, 15_000),
            QualityProfile::HighQuality => (5_000, 60_000, 25_000),
            QualityProfile::Adaptive => (2_000, 50_000, 15_000),
        };

        Self {
            profile,
            min_bitrate_kbps: min,
            max_bitrate_kbps: max,
            current_bitrate_kbps: initial,
            estimator: NetworkQualityEstimator::new(),
            last_tick_ms: 0,
        }
    }

    pub fn profile(&self) -> QualityProfile {
        self.profile
    }

    pub fn estimator_mut(&mut self) -> &mut NetworkQualityEstimator {
        &mut self.estimator
    }

    pub fn estimator(&self) -> &NetworkQualityEstimator {
        &self.estimator
    }

    pub fn current_bitrate_kbps(&self) -> u32 {
        self.current_bitrate_kbps
    }

    pub fn congestion_state(&self) -> CongestionState {
        let loss = self.estimator.smoothed_loss_percent();
        let rtt = self.estimator.smoothed_rtt_ms();

        if loss > 5.0 || rtt > 150.0 {
            CongestionState::CriticallyCongested
        } else if loss > 2.0 || rtt > 80.0 {
            CongestionState::Congested
        } else if loss > 0.5 || rtt > 40.0 {
            CongestionState::MildDelay
        } else {
            CongestionState::Optimal
        }
    }

    pub fn tick(&mut self, now_ms: u64) -> Option<u32> {
        if self.last_tick_ms > 0 && (now_ms - self.last_tick_ms) < 250 {
            return None;
        }
        self.last_tick_ms = now_ms;

        let state = self.congestion_state();
        let old_bitrate = self.current_bitrate_kbps;

        match state {
            CongestionState::CriticallyCongested => {
                // Multiplicative decrease 30%
                self.current_bitrate_kbps = (self.current_bitrate_kbps as f64 * 0.70) as u32;
            }
            CongestionState::Congested => {
                // Multiplicative decrease 15%
                self.current_bitrate_kbps = (self.current_bitrate_kbps as f64 * 0.85) as u32;
            }
            CongestionState::MildDelay => {
                // Minor step down
                self.current_bitrate_kbps = self.current_bitrate_kbps.saturating_sub(500);
            }
            CongestionState::Optimal => {
                // Additive increase +500 kbps
                self.current_bitrate_kbps = self.current_bitrate_kbps.saturating_add(500);
            }
        }

        self.current_bitrate_kbps = self
            .current_bitrate_kbps
            .clamp(self.min_bitrate_kbps, self.max_bitrate_kbps);

        if (self.current_bitrate_kbps as i64 - old_bitrate as i64).abs() >= 250 {
            Some(self.current_bitrate_kbps)
        } else {
            None
        }
    }
}
