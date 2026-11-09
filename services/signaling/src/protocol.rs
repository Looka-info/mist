use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum SignalingMessage {
    Register {
        session_id: String,
        role: String,
        #[serde(default)]
        token: String,
        #[serde(default)]
        guest_id: Option<String>,
    },
    Offer {
        session_id: String,
        sdp: String,
        #[serde(default)]
        target_guest_id: Option<String>,
    },
    Answer {
        session_id: String,
        sdp: String,
        #[serde(default)]
        guest_id: Option<String>,
    },
    Candidate {
        session_id: String,
        candidate: String,
        #[serde(default)]
        mid: Option<String>,
        #[serde(default)]
        guest_id: Option<String>,
    },
    Bye {
        session_id: String,
        #[serde(default)]
        guest_id: Option<String>,
    },
    Registered {
        session_id: String,
        role: String,
        peer_connected: bool,
        #[serde(default)]
        guest_id: Option<String>,
    },
    PeerJoined {
        session_id: String,
        peer_role: String,
        #[serde(default)]
        guest_id: Option<String>,
    },
    PeerLeft {
        session_id: String,
        #[serde(default)]
        guest_id: Option<String>,
    },
    Error {
        message: String,
    },
}
