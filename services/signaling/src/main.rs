use futures_util::{SinkExt, StreamExt};
use mist_signaling::audit_log::{log_audit, AuditEventType, AuditLogRecord};
use mist_signaling::connection_tracker::{ConnectionTrackerConfig, IpConnectionTracker};
use mist_signaling::protocol::SignalingMessage;
use mist_signaling::rate_limiter::{RateLimiter, RateLimiterConfig};
use mist_signaling::session_store::SessionStore;
use std::net::SocketAddr;
use std::sync::Arc;
use std::time::Duration;
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::{mpsc, Mutex, RwLock};
use tokio_tungstenite::tungstenite::Message;
use tracing::{info, warn};

type SharedStore = Arc<RwLock<SessionStore>>;
type SharedRateLimiter = Arc<Mutex<RateLimiter>>;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env().unwrap_or_else(|_| "info".into()),
        )
        .init();

    let mut bind_addr = "0.0.0.0:8080".to_string();
    let args: Vec<String> = std::env::args().collect();
    let mut i = 1;
    while i < args.len() {
        if args[i] == "--bind" && i + 1 < args.len() {
            bind_addr = args[i + 1].clone();
            i += 2;
        } else if args[i] == "--port" && i + 1 < args.len() {
            bind_addr = format!("0.0.0.0:{}", args[i + 1]);
            i += 2;
        } else if args[i] == "--help" || args[i] == "-h" {
            println!("Mist Hardened Signaling Service v0.3.0");
            println!("Usage: mist-signaling [--bind <IP:PORT>] [--port <PORT>]");
            return Ok(());
        } else {
            i += 1;
        }
    }

    let addr: SocketAddr = bind_addr.parse()?;
    let listener = TcpListener::bind(&addr).await?;
    info!("Mist Hardened Signaling server running on ws://{}", addr);

    let session_ttl = Duration::from_secs(3600); // 1 hour room TTL
    let session_store: SharedStore = Arc::new(RwLock::new(SessionStore::new(session_ttl)));

    let rate_limiter: SharedRateLimiter =
        Arc::new(Mutex::new(RateLimiter::new(RateLimiterConfig {
            max_requests_per_sec: 30,
            burst_capacity: 50,
        })));

    let connection_tracker = Arc::new(IpConnectionTracker::new(ConnectionTrackerConfig {
        max_connections_per_ip: 10,
        max_total_connections: 1024,
    }));

    // Background task: prune expired sessions every 60 seconds
    let store_cleaner = Arc::clone(&session_store);
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(60));
        loop {
            interval.tick().await;
            let mut store = store_cleaner.write().await;
            let pruned = store.prune_expired();
            if pruned > 0 {
                info!("Pruned {} expired signaling session(s)", pruned);
            }
        }
    });

    while let Ok((stream, client_addr)) = listener.accept().await {
        // Enforce IP connection limits
        let guard = match connection_tracker.try_accept(client_addr.ip()).await {
            Ok(g) => g,
            Err(reason) => {
                warn!("Connection rejected from {}: {}", client_addr, reason);
                log_audit(&AuditLogRecord::new(
                    AuditEventType::ConnectionRejected,
                    Some(client_addr.ip()),
                    None,
                    None,
                    reason,
                ));
                continue;
            }
        };

        log_audit(&AuditLogRecord::new(
            AuditEventType::ConnectionAccepted,
            Some(client_addr.ip()),
            None,
            None,
            "WebSocket connection accepted",
        ));

        let store_clone = Arc::clone(&session_store);
        let limiter_clone = Arc::clone(&rate_limiter);
        tokio::spawn(async move {
            let _guard = guard;
            handle_connection(stream, client_addr, store_clone, limiter_clone).await;
        });
    }

    Ok(())
}

async fn handle_connection(
    stream: TcpStream,
    addr: SocketAddr,
    sessions: SharedStore,
    rate_limiter: SharedRateLimiter,
) {
    let ws_stream = match tokio_tungstenite::accept_async(stream).await {
        Ok(ws) => ws,
        Err(e) => {
            warn!("WebSocket handshake failed with {}: {}", addr, e);
            return;
        }
    };

    let (mut ws_sender, mut ws_receiver) = ws_stream.split();
    let (tx, mut rx) = mpsc::unbounded_channel::<Message>();

    let outgoing_task = tokio::spawn(async move {
        while let Some(msg) = rx.recv().await {
            if ws_sender.send(msg).await.is_err() {
                break;
            }
        }
    });

    let mut registered_session_id: Option<String> = None;
    let mut registered_role: Option<String> = None;
    let mut registered_guest_id: Option<String> = None;

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

        // Rate limit check
        {
            let mut limiter = rate_limiter.lock().await;
            if !limiter.check_rate_limit(addr.ip()) {
                warn!("Rate limit exceeded for IP: {}", addr.ip());
                log_audit(&AuditLogRecord::new(
                    AuditEventType::RateLimitExceeded,
                    Some(addr.ip()),
                    registered_session_id.as_deref(),
                    registered_role.as_deref(),
                    "Too many signaling requests",
                ));
                let err_msg = SignalingMessage::Error {
                    message: "Rate limit exceeded. Please throttle requests.".to_string(),
                };
                if let Ok(json) = serde_json::to_string(&err_msg) {
                    let _ = tx.send(Message::Text(json));
                }
                continue;
            }
        }

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
            SignalingMessage::Register {
                session_id,
                role,
                token,
                guest_id,
            } => {
                let mut store = sessions.write().await;
                let session = store.get_or_create(&session_id, &token);

                if session.is_expired() {
                    let err = SignalingMessage::Error {
                        message: "Session room has expired".to_string(),
                    };
                    if let Ok(json) = serde_json::to_string(&err) {
                        let _ = tx.send(Message::Text(json));
                    }
                    log_audit(&AuditLogRecord::new(
                        AuditEventType::SessionExpired,
                        Some(addr.ip()),
                        Some(&session_id),
                        Some(&role),
                        "Attempted registration to expired room",
                    ));
                    continue;
                }

                if !session.token.is_empty() && !token.is_empty() && session.token != token {
                    let err = SignalingMessage::Error {
                        message: "Invalid session authorization token".to_string(),
                    };
                    if let Ok(json) = serde_json::to_string(&err) {
                        let _ = tx.send(Message::Text(json));
                    }
                    log_audit(&AuditLogRecord::new(
                        AuditEventType::AuthenticationFailed,
                        Some(addr.ip()),
                        Some(&session_id),
                        Some(&role),
                        "Room token mismatch",
                    ));
                    continue;
                }

                registered_session_id = Some(session_id.clone());
                registered_role = Some(role.clone());
                registered_guest_id = guest_id.clone();

                log_audit(&AuditLogRecord::new(
                    AuditEventType::PeerRegistered,
                    Some(addr.ip()),
                    Some(&session_id),
                    Some(&role),
                    format!("Peer registered as {} (guest={:?})", role, guest_id),
                ));

                let peer_connected = if role == "host" {
                    session.host_tx = Some(tx.clone());
                    // Notify any existing guests
                    for (g_id, g_info) in &session.guests {
                        let notification = SignalingMessage::PeerJoined {
                            session_id: session_id.clone(),
                            peer_role: "host".to_string(),
                            guest_id: Some(g_id.clone()),
                        };
                        if let Ok(json) = serde_json::to_string(&notification) {
                            let _ = g_info.tx.send(Message::Text(json));
                        }
                    }
                    !session.guests.is_empty()
                } else {
                    let effective_guest_id = guest_id.clone().unwrap_or_else(|| addr.to_string());
                    session.guests.insert(
                        effective_guest_id.clone(),
                        mist_signaling::GuestInfo {
                            guest_id: effective_guest_id.clone(),
                            tx: tx.clone(),
                            joined_at: std::time::Instant::now(),
                        },
                    );

                    if let Some(ref host_tx) = session.host_tx {
                        let notification = SignalingMessage::PeerJoined {
                            session_id: session_id.clone(),
                            peer_role: "client".to_string(),
                            guest_id: Some(effective_guest_id),
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
                    guest_id,
                };
                if let Ok(json) = serde_json::to_string(&reg_ack) {
                    let _ = tx.send(Message::Text(json));
                }
            }

            SignalingMessage::Offer {
                session_id,
                sdp,
                target_guest_id,
            } => {
                let store = sessions.read().await;
                if let Some(session) = store.get(&session_id) {
                    if let Some(ref target_id) = target_guest_id {
                        if let Some(guest) = session.guests.get(target_id) {
                            let forward = SignalingMessage::Offer {
                                session_id,
                                sdp,
                                target_guest_id,
                            };
                            if let Ok(json) = serde_json::to_string(&forward) {
                                let _ = guest.tx.send(Message::Text(json));
                            }
                        }
                    } else if let Some((_, guest)) = session.guests.iter().next() {
                        let forward = SignalingMessage::Offer {
                            session_id,
                            sdp,
                            target_guest_id,
                        };
                        if let Ok(json) = serde_json::to_string(&forward) {
                            let _ = guest.tx.send(Message::Text(json));
                        }
                    }
                }
            }

            SignalingMessage::Answer {
                session_id,
                sdp,
                guest_id,
            } => {
                let store = sessions.read().await;
                if let Some(session) = store.get(&session_id) {
                    if let Some(ref host_tx) = session.host_tx {
                        let forward = SignalingMessage::Answer {
                            session_id,
                            sdp,
                            guest_id,
                        };
                        if let Ok(json) = serde_json::to_string(&forward) {
                            let _ = host_tx.send(Message::Text(json));
                        }
                    }
                }
            }

            SignalingMessage::Candidate {
                session_id,
                candidate,
                mid,
                guest_id,
            } => {
                let store = sessions.read().await;
                if let Some(session) = store.get(&session_id) {
                    let forward = SignalingMessage::Candidate {
                        session_id,
                        candidate,
                        mid,
                        guest_id: guest_id.clone(),
                    };
                    if let Ok(json) = serde_json::to_string(&forward) {
                        if registered_role.as_deref() == Some("host") {
                            if let Some(ref target_id) = guest_id {
                                if let Some(guest) = session.guests.get(target_id) {
                                    let _ = guest.tx.send(Message::Text(json));
                                }
                            } else {
                                for guest in session.guests.values() {
                                    let _ = guest.tx.send(Message::Text(json.clone()));
                                }
                            }
                        } else if let Some(ref host_tx) = session.host_tx {
                            let _ = host_tx.send(Message::Text(json));
                        }
                    }
                }
            }

            SignalingMessage::Bye {
                session_id: _,
                guest_id: _,
            } => {
                break;
            }

            _ => {}
        }
    }

    // Cleanup session on disconnect
    if let (Some(ref session_id), Some(ref role)) = (&registered_session_id, &registered_role) {
        let mut store = sessions.write().await;
        let mut should_remove = false;
        if let Some(session) = store.get_mut(session_id) {
            if role == "host" {
                session.host_tx = None;
                for guest in session.guests.values() {
                    let notify = SignalingMessage::PeerLeft {
                        session_id: session_id.clone(),
                        guest_id: None,
                    };
                    if let Ok(json) = serde_json::to_string(&notify) {
                        let _ = guest.tx.send(Message::Text(json));
                    }
                }
                if session.guests.is_empty() {
                    should_remove = true;
                }
            } else {
                let g_id = registered_guest_id
                    .clone()
                    .unwrap_or_else(|| addr.to_string());
                session.guests.remove(&g_id);
                if let Some(ref host_tx) = session.host_tx {
                    let notify = SignalingMessage::PeerLeft {
                        session_id: session_id.clone(),
                        guest_id: Some(g_id),
                    };
                    if let Ok(json) = serde_json::to_string(&notify) {
                        let _ = host_tx.send(Message::Text(json));
                    }
                }
                if session.host_tx.is_none() && session.guests.is_empty() {
                    should_remove = true;
                }
            }
        }
        if should_remove {
            store.remove(session_id);
        }

        log_audit(&AuditLogRecord::new(
            AuditEventType::PeerLeft,
            Some(addr.ip()),
            Some(session_id),
            Some(role),
            format!("Peer disconnected (role={})", role),
        ));
    }

    outgoing_task.abort();
    info!("Connection closed from {}", addr);
}
