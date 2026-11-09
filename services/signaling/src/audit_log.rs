use serde::{Deserialize, Serialize};
use std::net::IpAddr;
use std::time::SystemTime;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum AuditEventType {
    ConnectionAccepted,
    ConnectionRejected,
    RateLimitExceeded,
    AuthenticationSuccess,
    AuthenticationFailed,
    PeerRegistered,
    PeerJoined,
    PeerLeft,
    SessionExpired,
    SessionClosed,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AuditLogRecord {
    pub timestamp_epoch_ms: u64,
    pub event_type: AuditEventType,
    pub client_ip: Option<String>,
    pub session_id: Option<String>,
    pub role: Option<String>,
    pub details: String,
}

impl AuditLogRecord {
    pub fn new(
        event_type: AuditEventType,
        client_ip: Option<IpAddr>,
        session_id: Option<&str>,
        role: Option<&str>,
        details: impl Into<String>,
    ) -> Self {
        let timestamp_epoch_ms = SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .map(|d| d.as_millis() as u64)
            .unwrap_or(0);

        Self {
            timestamp_epoch_ms,
            event_type,
            client_ip: client_ip.map(|ip| ip.to_string()),
            session_id: session_id.map(str::to_string),
            role: role.map(str::to_string),
            details: details.into(),
        }
    }

    pub fn to_json(&self) -> String {
        serde_json::to_string(self).unwrap_or_else(|_| "{}".to_string())
    }
}

pub fn log_audit(record: &AuditLogRecord) {
    tracing::info!(target: "mist_signaling::audit", "{}", record.to_json());
}
