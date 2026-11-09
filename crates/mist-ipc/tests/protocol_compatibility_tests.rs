use mist_ipc::{
    IpcMessage, IpcViewportPayload, IpcWindowPayload, MessageType, IPC_HEADER_SIZE, IPC_MAGIC,
    IPC_VERSION,
};

#[test]
fn test_ipc_binary_constants_and_layout() {
    assert_eq!(IPC_MAGIC, 0x4D495354);
    assert_eq!(IPC_VERSION, 1);
    assert_eq!(IPC_HEADER_SIZE, 20);

    assert_eq!(std::mem::size_of::<IpcWindowPayload>(), 16);
    assert_eq!(std::mem::size_of::<IpcViewportPayload>(), 16);
}

#[test]
fn test_golden_vector_heartbeat_roundtrip() {
    // Golden vector: Heartbeat with timestamp 10000000000 ns (0x02540BE400)
    #[rustfmt::skip]
    let golden_heartbeat: [u8; 20] = [
        0x54, 0x53, 0x49, 0x4D, // 'MIST'
        0x01, 0x00,             // Version 1
        0x01, 0x00,             // MessageType::Heartbeat (1)
        0x00, 0x00, 0x00, 0x00, // PayloadSize: 0
        0x00, 0xE4, 0x0B, 0x54, 0x02, 0x00, 0x00, 0x00, // Timestamp: 10000000000
    ];

    let msg = IpcMessage::deserialize(&golden_heartbeat).expect("Deserialization failed");
    assert_eq!(msg.msg_type, MessageType::Heartbeat);
    assert_eq!(msg.timestamp_ns, 10_000_000_000);
    assert!(msg.payload.is_empty());

    let serialized = msg.serialize();
    assert_eq!(serialized, golden_heartbeat);
}

#[test]
fn test_golden_vector_viewport_roundtrip() {
    // Golden vector: SetViewport (type 18), x=10, y=20, width=1920, height=1080, timestamp=0
    #[rustfmt::skip]
    let golden_viewport: [u8; 36] = [
        0x54, 0x53, 0x49, 0x4D, // 'MIST'
        0x01, 0x00,             // Version 1
        0x12, 0x00,             // MessageType::SetViewport (18)
        0x10, 0x00, 0x00, 0x00, // PayloadSize: 16
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp: 0
        // Payload (16 bytes)
        0x0A, 0x00, 0x00, 0x00, // x: 10
        0x14, 0x00, 0x00, 0x00, // y: 20
        0x80, 0x07, 0x00, 0x00, // width: 1920
        0x38, 0x04, 0x00, 0x00, // height: 1080
    ];

    let msg = IpcMessage::deserialize(&golden_viewport).expect("Deserialization failed");
    assert_eq!(msg.msg_type, MessageType::SetViewport);
    assert_eq!(msg.timestamp_ns, 0);
    assert_eq!(msg.payload.len(), 16);

    let payload: IpcViewportPayload = msg.get_payload().expect("Payload decode failed");
    assert_eq!(payload.x, 10);
    assert_eq!(payload.y, 20);
    assert_eq!(payload.width, 1920);
    assert_eq!(payload.height, 1080);

    let reserialized = msg.serialize();
    assert_eq!(reserialized, golden_viewport);
}

#[test]
fn test_ipc_rejection_of_corrupt_data() {
    // 1. Truncated header (< 20 bytes)
    let short_bytes = vec![0x54, 0x53, 0x49, 0x4D, 0x01, 0x00];
    assert!(IpcMessage::deserialize(&short_bytes).is_none());

    // 2. Corrupt magic number
    #[rustfmt::skip]
    let bad_magic: [u8; 20] = [
        0x58, 0x58, 0x58, 0x58, // 'XXXX'
        0x01, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ];
    assert!(IpcMessage::deserialize(&bad_magic).is_none());

    // 3. Invalid protocol version
    #[rustfmt::skip]
    let bad_version: [u8; 20] = [
        0x54, 0x53, 0x49, 0x4D,
        0x99, 0x00, // Version 153
        0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ];
    assert!(IpcMessage::deserialize(&bad_version).is_none());

    // 4. Truncated payload (header says 16 bytes, but 8 provided)
    #[rustfmt::skip]
    let truncated_payload: [u8; 28] = [
        0x54, 0x53, 0x49, 0x4D,
        0x01, 0x00, 0x12, 0x00,
        0x10, 0x00, 0x00, 0x00, // Header claims 16 bytes payload
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // Only 8 bytes present
    ];
    assert!(IpcMessage::deserialize(&truncated_payload).is_none());
}
