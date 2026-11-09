use mist_signaling::audit_log::{AuditEventType, AuditLogRecord};
use mist_signaling::connection_tracker::{ConnectionTrackerConfig, IpConnectionTracker};
use mist_signaling::protocol::SignalingMessage;
use mist_signaling::rate_limiter::{RateLimiter, RateLimiterConfig};
use mist_signaling::session_store::SessionStore;
use std::net::{IpAddr, Ipv4Addr};
use std::time::Duration;

#[test]
fn test_rate_limiter_burst_and_throttling() {
    let mut limiter = RateLimiter::new(RateLimiterConfig {
        max_requests_per_sec: 10,
        burst_capacity: 5,
    });

    let ip = IpAddr::V4(Ipv4Addr::new(192, 168, 1, 50));

    // First 5 requests within burst capacity must succeed
    for i in 0..5 {
        assert!(
            limiter.check_rate_limit(ip),
            "Request {} should be accepted under burst capacity",
            i
        );
    }

    // 6th request immediate attempt must be rejected (exhausted tokens)
    assert!(
        !limiter.check_rate_limit(ip),
        "6th request must be throttled"
    );

    // Sleep 150ms -> yields at least 1 token (10 tokens/sec = 1 token per 100ms)
    std::thread::sleep(Duration::from_millis(150));
    assert!(
        limiter.check_rate_limit(ip),
        "Request after token replenishment should succeed"
    );
}

#[tokio::test]
async fn test_connection_tracker_ip_limit() {
    let tracker = IpConnectionTracker::new(ConnectionTrackerConfig {
        max_connections_per_ip: 2,
        max_total_connections: 5,
    });

    let ip1 = IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1));
    let ip2 = IpAddr::V4(Ipv4Addr::new(10, 0, 0, 2));

    // IP1 connects 2 times -> ok
    let g1 = tracker.try_accept(ip1).await.expect("Conn 1 ok");
    let _g2 = tracker.try_accept(ip1).await.expect("Conn 2 ok");

    // IP1 3rd connection -> rejected
    let fail1 = tracker.try_accept(ip1).await;
    assert!(fail1.is_err());
    assert_eq!(fail1.err(), Some("Per-IP connection limit exceeded"));

    // IP2 can still connect
    let _g3 = tracker.try_accept(ip2).await.expect("IP2 conn ok");
    assert_eq!(tracker.total_connections(), 3);

    // Drop g1 -> IP1 count decrements
    drop(g1);
    tokio::time::sleep(Duration::from_millis(10)).await;

    // Now IP1 can connect again
    let _g4 = tracker.try_accept(ip1).await.expect("Conn 4 ok after drop");
}

#[test]
fn test_session_store_ttl_and_pruning() {
    let mut store = SessionStore::new(Duration::from_millis(50));

    store.get_or_create("room-fast-expire", "token-abc");
    assert_eq!(store.session_count(), 1);

    // Immediate check -> not expired
    assert!(!store.get("room-fast-expire").unwrap().is_expired());

    // Sleep past TTL
    std::thread::sleep(Duration::from_millis(60));
    assert!(store.get("room-fast-expire").unwrap().is_expired());

    // Prune expired
    let pruned = store.prune_expired();
    assert_eq!(pruned, 1);
    assert_eq!(store.session_count(), 0);
}

#[test]
fn test_audit_log_formatting() {
    let ip = IpAddr::V4(Ipv4Addr::new(172, 16, 0, 5));
    let record = AuditLogRecord::new(
        AuditEventType::AuthenticationFailed,
        Some(ip),
        Some("sess-test"),
        Some("client"),
        "Invalid token signature provided",
    );

    let json = record.to_json();
    assert!(json.contains("authentication_failed"));
    assert!(json.contains("172.16.0.5"));
    assert!(json.contains("sess-test"));
    assert!(json.contains("Invalid token signature"));
}

#[test]
fn test_protocol_multi_guest_serde() {
    let msg = SignalingMessage::Offer {
        session_id: "sess-123".to_string(),
        sdp: "v=0...".to_string(),
        target_guest_id: Some("guest-99".to_string()),
    };

    let json = serde_json::to_string(&msg).unwrap();
    assert!(json.contains("target_guest_id"));

    // Deserialization roundtrip
    let deserialized: SignalingMessage = serde_json::from_str(&json).unwrap();
    match deserialized {
        SignalingMessage::Offer { session_id, sdp, target_guest_id } => {
            assert_eq!(session_id, "sess-123");
            assert_eq!(sdp, "v=0...");
            assert_eq!(target_guest_id, Some("guest-99".to_string()));
        }
        _ => panic!("Expected Offer variant"),
    }

    // Backwards compatibility: deserialize message without target_guest_id
    let legacy_json = r#"{"type":"offer","session_id":"sess-old","sdp":"v=0..."}"#;
    let legacy: SignalingMessage = serde_json::from_str(legacy_json).unwrap();
    match legacy {
        SignalingMessage::Offer { session_id, target_guest_id, .. } => {
            assert_eq!(session_id, "sess-old");
            assert_eq!(target_guest_id, None);
        }
        _ => panic!("Expected Offer variant"),
    }
}
