use std::collections::HashMap;
use std::net::SocketAddr;
use std::sync::Arc;
use futures_util::{SinkExt, StreamExt};
use serde::{Deserialize, Serialize};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::{mpsc, RwLock};
use tokio_tungstenite::tungstenite::Message;
use tracing::{info, warn};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum SignalingMessage {
    Register {
        session_id: String,
        role: String,
        #[serde(default)]
        token: String,
    },
    Offer {
        session_id: String,
        sdp: String,
    },
    Answer {
        session_id: String,
        sdp: String,
    },
    Candidate {
        session_id: String,
        candidate: String,
        #[serde(default)]
        mid: Option<String>,
    },
    Bye {
        session_id: String,
    },
    Registered {
        session_id: String,
        role: String,
        peer_connected: bool,
    },
    PeerJoined {
        session_id: String,
        peer_role: String,
    },
    PeerLeft {
        session_id: String,
    },
    Error {
        message: String,
    },
}

struct Session {
    token: String,
    host_tx: Option<mpsc::UnboundedSender<Message>>,
    client_tx: Option<mpsc::UnboundedSender<Message>>,
}

type SessionMap = Arc<RwLock<HashMap<String, Session>>>;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "info".into()),
        )
        .init();

    let mut bind_addr = "127.0.0.1:8080".to_string();
    let args: Vec<String> = std::env::args().collect();
    let mut i = 1;
    while i < args.len() {
        if args[i] == "--bind" && i + 1 < args.len() {
            bind_addr = args[i + 1].clone();
            i += 2;
        } else if args[i] == "--port" && i + 1 < args.len() {
            bind_addr = format!("127.0.0.1:{}", args[i + 1]);
            i += 2;
        } else if args[i] == "--help" || args[i] == "-h" {
            println!("Mist Signaling Service v0.2.0-rc1");
            println!("Usage: mist-signaling [--bind <IP:PORT>] [--port <PORT>]");
            return Ok(());
        } else {
            i += 1;
        }
    }

    let addr: SocketAddr = bind_addr.parse()?;
    let listener = TcpListener::bind(&addr).await?;
    info!("Mist Signaling server running on ws://{}", addr);

    let sessions: SessionMap = Arc::new(RwLock::new(HashMap::new()));

    while let Ok((stream, client_addr)) = listener.accept().await {
        let sessions_clone = Arc::clone(&sessions);
        tokio::spawn(handle_connection(stream, client_addr, sessions_clone));
    }

    Ok(())
}

async fn handle_connection(stream: TcpStream, addr: SocketAddr, sessions: SessionMap) {
    let ws_stream = match tokio_tungstenite::accept_async(stream).await {
        Ok(ws) => ws,
        Err(e) => {
            warn!("WebSocket handshake failed with {}: {}", addr, e);
            return;
        }
    };

    let (mut ws_sender, mut ws_receiver) = ws_stream.split();
    let (tx, mut rx) = mpsc::unbounded_channel::<Message>();

    // Forward outgoing messages from mpsc queue to WebSocket sender
    let outgoing_task = tokio::spawn(async move {
        while let Some(msg) = rx.recv().await {
            if ws_sender.send(msg).await.is_err() {
                break;
            }
        }
    });

    let mut registered_session_id: Option<String> = None;
    let mut registered_role: Option<String> = None;

    while let Some(msg_result) = ws_receiver.next().await {
        let msg = match msg_result {
            Ok(Message::Text(text)) => text,
            Ok(Message::Close(_)) => break,
            Ok(Message::Ping(data)) => {
                let _ = tx.send(Message::Pong(data));
                continue;
            }
            Ok(_) => continue,
            Err(e) => {
                warn!("WebSocket read error from {}: {}", addr, e);
                break;
            }
        };

        let parsed: Result<SignalingMessage, _> = serde_json::from_str(&msg);
        let signaling_msg = match parsed {
            Ok(m) => m,
            Err(e) => {
                warn!("Malformed JSON from {}: {}", addr, e);
                let err_msg = SignalingMessage::Error {
                    message: format!("Invalid JSON: {}", e),
                };
                if let Ok(json) = serde_json::to_string(&err_msg) {
                    let _ = tx.send(Message::Text(json));
                }
                continue;
            }
        };

        match signaling_msg {
            SignalingMessage::Register { session_id, role, token } => {
                let mut map = sessions.write().await;
                let session = map.entry(session_id.clone()).or_insert_with(|| Session {
                    token: token.clone(),
                    host_tx: None,
                    client_tx: None,
                });

                if !session.token.is_empty() && !token.is_empty() && session.token != token {
                    let err = SignalingMessage::Error {
                        message: "Invalid session authorization token".to_string(),
                    };
                    if let Ok(json) = serde_json::to_string(&err) {
                        let _ = tx.send(Message::Text(json));
                    }
                    continue;
                }

                registered_session_id = Some(session_id.clone());
                registered_role = Some(role.clone());

                let peer_connected = if role == "host" {
                    session.host_tx = Some(tx.clone());
                    if let Some(ref client_tx) = session.client_tx {
                        let notification = SignalingMessage::PeerJoined {
                            session_id: session_id.clone(),
                            peer_role: "host".to_string(),
                        };
                        if let Ok(json) = serde_json::to_string(&notification) {
                            let _ = client_tx.send(Message::Text(json));
                        }
                        true
                    } else {
                        false
                    }
                } else {
                    session.client_tx = Some(tx.clone());
                    if let Some(ref host_tx) = session.host_tx {
                        let notification = SignalingMessage::PeerJoined {
                            session_id: session_id.clone(),
                            peer_role: "client".to_string(),
                        };
                        if let Ok(json) = serde_json::to_string(&notification) {
                            let _ = host_tx.send(Message::Text(json));
                        }
                        true
                    } else {
                        false
                    }
                };

                let reg_ack = SignalingMessage::Registered {
                    session_id,
                    role,
                    peer_connected,
                };
                if let Ok(json) = serde_json::to_string(&reg_ack) {
                    let _ = tx.send(Message::Text(json));
                }
            }

            SignalingMessage::Offer { session_id, sdp } => {
                let map = sessions.read().await;
                if let Some(session) = map.get(&session_id) {
                    if let Some(ref client_tx) = session.client_tx {
                        let forward = SignalingMessage::Offer { session_id, sdp };
                        if let Ok(json) = serde_json::to_string(&forward) {
                            let _ = client_tx.send(Message::Text(json));
                        }
                    } else {
                        let err = SignalingMessage::Error {
                            message: "Client not connected to session".to_string(),
                        };
                        if let Ok(json) = serde_json::to_string(&err) {
                            let _ = tx.send(Message::Text(json));
                        }
                    }
                }
            }

            SignalingMessage::Answer { session_id, sdp } => {
                let map = sessions.read().await;
                if let Some(session) = map.get(&session_id) {
                    if let Some(ref host_tx) = session.host_tx {
                        let forward = SignalingMessage::Answer { session_id, sdp };
                        if let Ok(json) = serde_json::to_string(&forward) {
                            let _ = host_tx.send(Message::Text(json));
                        }
                    } else {
                        let err = SignalingMessage::Error {
                            message: "Host not connected to session".to_string(),
                        };
                        if let Ok(json) = serde_json::to_string(&err) {
                            let _ = tx.send(Message::Text(json));
                        }
                    }
                }
            }

            SignalingMessage::Candidate { session_id, candidate, mid } => {
                let map = sessions.read().await;
                if let Some(session) = map.get(&session_id) {
                    let forward = SignalingMessage::Candidate {
                        session_id,
                        candidate,
                        mid,
                    };
                    if let Ok(json) = serde_json::to_string(&forward) {
                        if registered_role.as_deref() == Some("host") {
                            if let Some(ref client_tx) = session.client_tx {
                                let _ = client_tx.send(Message::Text(json));
                            }
                        } else if let Some(ref host_tx) = session.host_tx {
                            let _ = host_tx.send(Message::Text(json));
                        }
                    }
                }
            }

            SignalingMessage::Bye { session_id: _ } => {
                break;
            }

            _ => {}
        }
    }

    // Cleanup session on disconnect
    if let (Some(ref session_id), Some(ref role)) = (&registered_session_id, &registered_role) {
        let mut map = sessions.write().await;
        let mut should_remove = false;
        if let Some(session) = map.get_mut(session_id) {
            if role == "host" {
                session.host_tx = None;
                if let Some(ref client_tx) = session.client_tx {
                    let notify = SignalingMessage::PeerLeft {
                        session_id: session_id.clone(),
                    };
                    if let Ok(json) = serde_json::to_string(&notify) {
                        let _ = client_tx.send(Message::Text(json));
                    }
                } else {
                    should_remove = true;
                }
            } else {
                session.client_tx = None;
                if let Some(ref host_tx) = session.host_tx {
                    let notify = SignalingMessage::PeerLeft {
                        session_id: session_id.clone(),
                    };
                    if let Ok(json) = serde_json::to_string(&notify) {
                        let _ = host_tx.send(Message::Text(json));
                    }
                } else {
                    should_remove = true;
                }
            }
        }
        if should_remove {
            map.remove(session_id);
        }
    }

    outgoing_task.abort();
    info!("Connection closed from {}", addr);
}
