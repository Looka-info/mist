//! Leaky/token bucket packet pacer to prevent packet bursts and network congestion.

use std::collections::VecDeque;

#[derive(Debug, Clone, PartialEq)]
pub struct LowLatencyProfile {
    pub pacing_factor: f64,
    pub max_queue_packets: usize,
    pub max_queue_duration_ms: u32,
}

impl Default for LowLatencyProfile {
    fn default() -> Self {
        Self {
            pacing_factor: 1.25,
            max_queue_packets: 500,
            max_queue_duration_ms: 50,
        }
    }
}

struct PacedPacket {
    data: Vec<u8>,
    enqueue_time_ns: u64,
}

pub struct PacedSender {
    profile: LowLatencyProfile,
    target_bitrate_kbps: u32,
    max_token_bucket_bytes: f64,
    token_bucket_bytes: f64,
    last_token_update_time_ns: u64,

    high_priority_queue: VecDeque<Vec<u8>>,
    video_queue: VecDeque<PacedPacket>,

    packets_sent: u64,
    packets_dropped: u64,
}

impl Default for PacedSender {
    fn default() -> Self {
        Self::new(LowLatencyProfile::default())
    }
}

impl PacedSender {
    pub fn new(profile: LowLatencyProfile) -> Self {
        let mut sender = Self {
            profile,
            target_bitrate_kbps: 15_000,
            max_token_bucket_bytes: 16384.0,
            token_bucket_bytes: 16384.0,
            last_token_update_time_ns: 0,
            high_priority_queue: VecDeque::new(),
            video_queue: VecDeque::new(),
            packets_sent: 0,
            packets_dropped: 0,
        };
        sender.set_pacing_rate(15_000);
        sender
    }

    pub fn set_pacing_rate(&mut self, target_bitrate_kbps: u32) {
        self.target_bitrate_kbps = target_bitrate_kbps.max(500);
        let bytes_per_sec =
            (self.target_bitrate_kbps as f64 * 1000.0 * self.profile.pacing_factor) / 8.0;
        self.max_token_bucket_bytes = (bytes_per_sec * 0.010).max(16384.0);
        self.token_bucket_bytes = self.max_token_bucket_bytes;
    }

    pub fn enqueue_packet(
        &mut self,
        data: Vec<u8>,
        enqueue_time_ns: u64,
        is_high_priority: bool,
    ) -> bool {
        if data.is_empty() {
            return false;
        }

        if self.high_priority_queue.len() + self.video_queue.len() >= self.profile.max_queue_packets
        {
            if !self.video_queue.is_empty() {
                self.video_queue.pop_front();
                self.packets_dropped += 1;
            } else {
                self.packets_dropped += 1;
                return false;
            }
        }

        if is_high_priority {
            self.high_priority_queue.push_back(data);
        } else {
            self.video_queue.push_back(PacedPacket {
                data,
                enqueue_time_ns,
            });
        }

        true
    }

    pub fn get_next_packet_to_transmit(&mut self, now_ns: u64) -> Option<Vec<u8>> {
        // 1. High-priority packets always bypass pacing delay
        if let Some(pkt) = self.high_priority_queue.pop_front() {
            self.packets_sent += 1;
            return Some(pkt);
        }

        if self.video_queue.is_empty() {
            return None;
        }

        // 2. Replenish token bucket based on elapsed time
        if self.last_token_update_time_ns == 0 {
            self.last_token_update_time_ns = now_ns;
        }

        if now_ns > self.last_token_update_time_ns {
            let elapsed_sec = (now_ns - self.last_token_update_time_ns) as f64 / 1_000_000_000.0;
            let bytes_per_sec =
                (self.target_bitrate_kbps as f64 * 1000.0 * self.profile.pacing_factor) / 8.0;
            self.token_bucket_bytes = (self.token_bucket_bytes + (elapsed_sec * bytes_per_sec))
                .min(self.max_token_bucket_bytes);
            self.last_token_update_time_ns = now_ns;
        }

        // 3. Drop stale video packets exceeding max_queue_duration_ms
        let max_queue_duration_ns = self.profile.max_queue_duration_ms as u64 * 1_000_000;
        while let Some(front) = self.video_queue.front() {
            if now_ns >= front.enqueue_time_ns
                && (now_ns - front.enqueue_time_ns) > max_queue_duration_ns
            {
                self.video_queue.pop_front();
                self.packets_dropped += 1;
            } else {
                break;
            }
        }

        // 4. Send video packet if enough tokens available
        if let Some(front) = self.video_queue.front() {
            let pkt_len = front.data.len() as f64;
            if self.token_bucket_bytes >= pkt_len
                || self.token_bucket_bytes >= self.max_token_bucket_bytes * 0.5
            {
                self.token_bucket_bytes -= pkt_len;
                let pkt = self.video_queue.pop_front().unwrap();
                self.packets_sent += 1;
                return Some(pkt.data);
            }
        }

        None
    }

    pub fn packets_sent(&self) -> u64 {
        self.packets_sent
    }

    pub fn packets_dropped(&self) -> u64 {
        self.packets_dropped
    }
}
