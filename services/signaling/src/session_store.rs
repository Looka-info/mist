use std::collections::HashMap;
use std::time::{Duration, Instant};
use tokio::sync::mpsc;
use tokio_tungstenite::tungstenite::Message;

pub struct GuestInfo {
    pub guest_id: String,
    pub tx: mpsc::UnboundedSender<Message>,
    pub joined_at: Instant,
}

pub struct HardenedSession {
    pub session_id: String,
    pub token: String,
    pub created_at: Instant,
    pub expires_at: Instant,
    pub host_tx: Option<mpsc::UnboundedSender<Message>>,
    pub guests: HashMap<String, GuestInfo>,
}

impl HardenedSession {
    pub fn new(session_id: String, token: String, ttl: Duration) -> Self {
        let now = Instant::now();
        Self {
            session_id,
            token,
            created_at: now,
            expires_at: now + ttl,
            host_tx: None,
            guests: HashMap::new(),
        }
    }

    pub fn is_expired(&self) -> bool {
        Instant::now() >= self.expires_at
    }

    pub fn refresh_expiry(&mut self, ttl: Duration) {
        self.expires_at = Instant::now() + ttl;
    }

    pub fn has_active_peers(&self) -> bool {
        self.host_tx.is_some() || !self.guests.is_empty()
    }
}

pub struct SessionStore {
    sessions: HashMap<String, HardenedSession>,
    default_ttl: Duration,
}

impl SessionStore {
    pub fn new(default_ttl: Duration) -> Self {
        Self {
            sessions: HashMap::new(),
            default_ttl,
        }
    }

    pub fn get_or_create(&mut self, session_id: &str, token: &str) -> &mut HardenedSession {
        self.sessions
            .entry(session_id.to_string())
            .or_insert_with(|| {
                HardenedSession::new(session_id.to_string(), token.to_string(), self.default_ttl)
            })
    }

    pub fn get(&self, session_id: &str) -> Option<&HardenedSession> {
        self.sessions.get(session_id)
    }

    pub fn get_mut(&mut self, session_id: &str) -> Option<&mut HardenedSession> {
        self.sessions.get_mut(session_id)
    }

    pub fn remove(&mut self, session_id: &str) -> Option<HardenedSession> {
        self.sessions.remove(session_id)
    }

    pub fn prune_expired(&mut self) -> usize {
        let initial_count = self.sessions.len();
        self.sessions.retain(|_, session| !session.is_expired());
        initial_count - self.sessions.len()
    }

    pub fn session_count(&self) -> usize {
        self.sessions.len()
    }
}
