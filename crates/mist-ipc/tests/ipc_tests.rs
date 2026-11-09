use mist_ipc::{IpcConfigPayload, IpcMessage, IpcTelemetryPayload, MessageType};

#[test]
fn test_ipc_message_serialization() {
    let telem = IpcTelemetryPayload {
        bitrate_kbps: 15000,
        rtt_ms: 6.2,
        jitter_ms: 0.8,
        fps: 60.0,
        packet_loss_percent: 0.0,
        frames_decoded: 3600,
        frames_dropped: 0,
        stats_timestamp_ns: 1234567890,
    };

    let msg = IpcMessage::with_payload(MessageType::TelemetryUpdate, &telem);
    let bytes = msg.serialize();

    let deserialized = IpcMessage::deserialize(&bytes).expect("Should deserialize message");
    assert_eq!(deserialized.msg_type, MessageType::TelemetryUpdate);

    let p = deserialized
        .get_payload::<IpcTelemetryPayload>()
        .expect("Payload should match");
    assert_eq!(p.bitrate_kbps, 15000);
    assert_eq!(p.frames_decoded, 3600);
    assert_eq!(p.stats_timestamp_ns, 1234567890);
}

#[test]
fn test_ipc_config_payload() {
    let cfg = IpcConfigPayload {
        bitrate_kbps: 20000,
        fps: 120,
        width: 2560,
        height: 1440,
    };

    let msg = IpcMessage::with_payload(MessageType::UpdateConfig, &cfg);
    let bytes = msg.serialize();

    let deserialized = IpcMessage::deserialize(&bytes).expect("Should deserialize");
    assert_eq!(deserialized.msg_type, MessageType::UpdateConfig);

    let p = deserialized
        .get_payload::<IpcConfigPayload>()
        .expect("Payload should match");
    assert_eq!(p.bitrate_kbps, 20000);
    assert_eq!(p.fps, 120);
    assert_eq!(p.width, 2560);
    assert_eq!(p.height, 1440);
}
