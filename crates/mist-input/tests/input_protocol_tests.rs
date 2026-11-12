use mist_domain::DevicePermissions;
use mist_input::*;

#[test]
fn test_input_packet_header_size_and_fields() {
    assert_eq!(std::mem::size_of::<InputPacketHeader>(), 48);
    let header = InputPacketHeader::new(InputType::KeyEvent, 42, 1_000_000_000, "session-test-123");
    assert_eq!(header.version, INPUT_PROTOCOL_VERSION);
    assert_eq!(header.sequence_number, 42);
    assert_eq!(header.timestamp_ns, 1_000_000_000);
    assert_eq!(header.session_id_str(), "session-test-123");
}

#[test]
fn test_key_event_roundtrip() {
    let header = InputPacketHeader::new(InputType::KeyEvent, 101, 555_000, "guest-1");
    let event = KeyEvent {
        virtual_key: 0x41, // 'A'
        scan_code: 0x1E,
        action: KeyAction::Press,
        is_extended: false,
    };

    let serialized = InputProtocol::serialize_key_event(header, &event);
    assert_eq!(serialized.len(), 48 + 12);

    let (hdr, parsed) = InputProtocol::deserialize(&serialized).expect("deserialize failed");
    assert_eq!(hdr.sequence_number, 101);
    assert_eq!(hdr.session_id_str(), "guest-1");

    match parsed {
        InputEvent::Key(k) => {
            assert_eq!(k.virtual_key, 0x41);
            assert_eq!(k.scan_code, 0x1E);
            assert_eq!(k.action, KeyAction::Press);
            assert!(!k.is_extended);
        }
        _ => panic!("Expected InputEvent::Key"),
    }
}

#[test]
fn test_mouse_move_roundtrip() {
    let header = InputPacketHeader::new(InputType::MouseMoveEvent, 102, 600_000, "guest-1");
    let event = MouseMoveEvent {
        delta_x: -15,
        delta_y: 25,
        is_absolute: true,
        normalized_x: 0.5,
        normalized_y: 0.75,
    };

    let serialized = InputProtocol::serialize_mouse_move_event(header, &event);
    assert_eq!(serialized.len(), 48 + 20);

    let (_, parsed) = InputProtocol::deserialize(&serialized).expect("deserialize failed");
    match parsed {
        InputEvent::MouseMove(m) => {
            assert_eq!(m.delta_x, -15);
            assert_eq!(m.delta_y, 25);
            assert!(m.is_absolute);
            assert!((m.normalized_x - 0.5).abs() < 1e-6);
            assert!((m.normalized_y - 0.75).abs() < 1e-6);
        }
        _ => panic!("Expected InputEvent::MouseMove"),
    }
}

#[test]
fn test_mouse_button_and_wheel_roundtrip() {
    let header = InputPacketHeader::new(InputType::MouseButtonEvent, 103, 700_000, "guest-1");
    let btn_event = MouseButtonEvent {
        button: MouseButton::Right,
        is_down: true,
    };
    let serialized_btn = InputProtocol::serialize_mouse_button_event(header, &btn_event);
    assert_eq!(serialized_btn.len(), 48 + 4);
    let (_, parsed_btn) = InputProtocol::deserialize(&serialized_btn).unwrap();
    match parsed_btn {
        InputEvent::MouseButton(b) => {
            assert_eq!(b.button, MouseButton::Right);
            assert!(b.is_down);
        }
        _ => panic!("Expected MouseButton"),
    }

    let wheel_event = MouseWheelEvent {
        wheel_delta_x: 0,
        wheel_delta_y: 120,
    };
    let serialized_wheel = InputProtocol::serialize_mouse_wheel_event(header, &wheel_event);
    assert_eq!(serialized_wheel.len(), 48 + 8);
    let (_, parsed_wheel) = InputProtocol::deserialize(&serialized_wheel).unwrap();
    match parsed_wheel {
        InputEvent::MouseWheel(w) => {
            assert_eq!(w.wheel_delta_x, 0);
            assert_eq!(w.wheel_delta_y, 120);
        }
        _ => panic!("Expected MouseWheel"),
    }
}

#[test]
fn test_gamepad_state_roundtrip() {
    let header = InputPacketHeader::new(InputType::GamepadState, 104, 800_000, "guest-2");
    let state = GamepadState {
        index: 1,
        buttons: 0x1004,
        left_trigger: 250,
        right_trigger: 120,
        thumb_lx: -15000,
        thumb_ly: 20000,
        thumb_rx: 500,
        thumb_ry: -800,
        is_connected: true,
    };

    let serialized = InputProtocol::serialize_gamepad_state(header, &state);
    assert_eq!(serialized.len(), 48 + 20);

    let (_, parsed) = InputProtocol::deserialize(&serialized).unwrap();
    match parsed {
        InputEvent::Gamepad(g) => {
            assert_eq!(g.index, 1);
            assert_eq!(g.buttons, 0x1004);
            assert_eq!(g.left_trigger, 250);
            assert_eq!(g.right_trigger, 120);
            assert_eq!(g.thumb_lx, -15000);
            assert_eq!(g.thumb_ly, 20000);
            assert!(g.is_connected);
        }
        _ => panic!("Expected Gamepad"),
    }
}

#[test]
fn test_clipboard_event_and_fnv1a() {
    let header = InputPacketHeader::new(InputType::ClipboardEvent, 105, 900_000, "guest-1");
    let text = "Hello from Mist Remote Gaming!";
    let serialized = InputProtocol::serialize_clipboard_event(header, text);

    let (_, parsed) = InputProtocol::deserialize(&serialized).unwrap();
    match parsed {
        InputEvent::Clipboard(cb) => {
            assert_eq!(cb.text, text);
            assert_eq!(cb.hash, compute_fnv1a(text));
        }
        _ => panic!("Expected Clipboard"),
    }
}

#[test]
fn test_permission_manager_rules() {
    let mgr = InputPermissionManager::new("");
    let guest = "guest-alice";

    // View only: no keyboard, mouse or gamepad
    mgr.set_guest_permissions(guest, DevicePermissions::view_only());
    assert!(!mgr.is_input_allowed(InputType::KeyEvent, guest));
    assert!(!mgr.is_input_allowed(InputType::MouseMoveEvent, guest));
    assert!(!mgr.is_input_allowed(InputType::GamepadState, guest));

    // Controller only
    mgr.set_guest_permissions(guest, DevicePermissions::gamepad_only());
    assert!(!mgr.is_input_allowed(InputType::KeyEvent, guest));
    assert!(mgr.is_input_allowed(InputType::GamepadState, guest));

    // Full control
    mgr.set_guest_permissions(guest, DevicePermissions::all());
    assert!(mgr.is_input_allowed(InputType::KeyEvent, guest));
    assert!(mgr.is_input_allowed(InputType::MouseMoveEvent, guest));
    assert!(mgr.is_input_allowed(InputType::GamepadState, guest));

    // Local host override cuts off all guest input immediately
    mgr.set_local_override(true);
    assert!(!mgr.is_input_allowed(InputType::KeyEvent, guest));
    assert!(!mgr.is_input_allowed(InputType::GamepadState, guest));
}
