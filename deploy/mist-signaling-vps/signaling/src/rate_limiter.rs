use std::collections::HashMap;
use std::net::IpAddr;
use std::time::{Duration, Instant};

#[derive(Debug, Clone)]
pub struct RateLimiterConfig {
    pub max_requests_per_sec: u32,
    pub burst_capacity: u32,
}

impl Default for RateLimiterConfig {
    fn default() -> Self {
        Self {
            max_requests_per_sec: 30,
            burst_capacity: 50,
        }
    }
}

struct TokenBucket {
    tokens: f64,
    last_update: Instant,
}

pub struct RateLimiter {
    config: RateLimiterConfig,
    buckets: HashMap<IpAddr, TokenBucket>,
}

impl RateLimiter {
    pub fn new(config: RateLimiterConfig) -> Self {
        Self {
            config,
            buckets: HashMap::new(),
        }
    }

    pub fn check_rate_limit(&mut self, ip: IpAddr) -> bool {
        let now = Instant::now();
        let burst = self.config.burst_capacity as f64;
        let rate = self.config.max_requests_per_sec as f64;

        let bucket = self.buckets.entry(ip).or_insert_with(|| TokenBucket {
            tokens: burst,
            last_update: now,
        });

        let elapsed = now.duration_since(bucket.last_update).as_secs_f64();
        bucket.last_update = now;

        // Refill tokens
        bucket.tokens = (bucket.tokens + elapsed * rate).min(burst);

        if bucket.tokens >= 1.0 {
            bucket.tokens -= 1.0;
            true
        } else {
            false
        }
    }

    pub fn prune_idle(&mut self, idle_timeout: Duration) {
        let now = Instant::now();
        self.buckets
            .retain(|_, bucket| now.duration_since(bucket.last_update) < idle_timeout);
    }

    pub fn active_tracked_ips(&self) -> usize {
        self.buckets.len()
    }
}
