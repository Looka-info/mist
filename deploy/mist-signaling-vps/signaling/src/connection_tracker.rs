use std::collections::HashMap;
use std::net::IpAddr;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use tokio::sync::Mutex;

#[derive(Debug, Clone)]
pub struct ConnectionTrackerConfig {
    pub max_connections_per_ip: usize,
    pub max_total_connections: usize,
}

impl Default for ConnectionTrackerConfig {
    fn default() -> Self {
        Self {
            max_connections_per_ip: 10,
            max_total_connections: 1024,
        }
    }
}

pub struct IpConnectionTracker {
    config: ConnectionTrackerConfig,
    ip_counts: Arc<Mutex<HashMap<IpAddr, usize>>>,
    total_connections: Arc<AtomicUsize>,
}

pub struct ConnectionGuard {
    ip: IpAddr,
    ip_counts: Arc<Mutex<HashMap<IpAddr, usize>>>,
    total_connections: Arc<AtomicUsize>,
}

impl Drop for ConnectionGuard {
    fn drop(&mut self) {
        self.total_connections.fetch_sub(1, Ordering::SeqCst);
        let counts = Arc::clone(&self.ip_counts);
        let ip = self.ip;
        tokio::spawn(async move {
            let mut lock = counts.lock().await;
            if let Some(count) = lock.get_mut(&ip) {
                if *count > 1 {
                    *count -= 1;
                } else {
                    lock.remove(&ip);
                }
            }
        });
    }
}

impl IpConnectionTracker {
    pub fn new(config: ConnectionTrackerConfig) -> Self {
        Self {
            config,
            ip_counts: Arc::new(Mutex::new(HashMap::new())),
            total_connections: Arc::new(AtomicUsize::new(0)),
        }
    }

    pub async fn try_accept(&self, ip: IpAddr) -> Result<ConnectionGuard, &'static str> {
        let current_total = self.total_connections.load(Ordering::SeqCst);
        if current_total >= self.config.max_total_connections {
            return Err("Global connection limit reached");
        }

        let mut lock = self.ip_counts.lock().await;
        let count = lock.entry(ip).or_insert(0);
        if *count >= self.config.max_connections_per_ip {
            return Err("Per-IP connection limit exceeded");
        }

        *count += 1;
        self.total_connections.fetch_add(1, Ordering::SeqCst);

        Ok(ConnectionGuard {
            ip,
            ip_counts: Arc::clone(&self.ip_counts),
            total_connections: Arc::clone(&self.total_connections),
        })
    }

    pub fn total_connections(&self) -> usize {
        self.total_connections.load(Ordering::SeqCst)
    }

    pub async fn get_ip_connection_count(&self, ip: IpAddr) -> usize {
        let lock = self.ip_counts.lock().await;
        lock.get(&ip).copied().unwrap_or(0)
    }
}
