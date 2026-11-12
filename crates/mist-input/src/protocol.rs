//! Binary protocol serialization and deserialization for Mist input events.

use crate::types::*;
use mist_core::{MistError, Result};

const FNV1A_OFFSET: u64 = 14695981039346656037;
const FNV1A_PRIME: u64 = 1099511628211;

pub fn compute_fnv1a(text: &str) -> u64 {
    let mut hash = FNV1A_OFFSET;
    for &b in text.as_bytes() {
        hash ^= b as u64;
        hash = hash.wrapping_mul(FNV1A_PRIME);
    }
    hash
}

pub struct InputProtocol;

impl InputProtocol {
    fn pack_header(mut header: InputPacketHeader, input_type: InputType) -> [u8; 48] {
        header.input_type = input_type as u8;
        let mut buf = [0u8; 48];
        buf[0..2].copy_from_slice(&header.version.to_le_bytes());
        buf[2] = header.input_type;
        buf[3] = header.flags;
        buf[4..8].copy_from_slice(&header.sequence_number.to_le_bytes());
        buf[8..16].copy_from_slice(&header.timestamp_ns.to_le_bytes());
        buf[16..48].copy_from_slice(&header.session_id);
        buf
    }

    pub fn serialize_key_event(header: InputPacketHeader, event: &KeyEvent) -> Vec<u8> {
        let mut buf = Vec::with_capacity(48 + 12);
        buf.extend_from_slice(&Self::pack_header(header, InputType::KeyEvent));
        buf.extend_from_slice(&event.virtual_key.to_le_bytes());
        buf.extend_from_slice(&event.scan_code.to_le_bytes());
        buf.push(event.action as u8);
        buf.push(if event.is_extended { 1 } else { 0 });
        buf.extend_from_slice(&0u16.to_le_bytes()); // reserved
        buf
    }

    pub fn serialize_mouse_move_event(
        header: InputPacketHeader,
        event: &MouseMoveEvent,
    ) -> Vec<u8> {
        let mut buf = Vec::with_capacity(48 + 20);
        buf.extend_from_slice(&Self::pack_header(header, InputType::MouseMoveEvent));
        buf.extend_from_slice(&event.delta_x.to_le_bytes());
        buf.extend_from_slice(&event.delta_y.to_le_bytes());
        buf.push(if event.is_absolute { 1 } else { 0 });
        buf.extend_from_slice(&[0u8; 3]); // reserved
        buf.extend_from_slice(&event.normalized_x.to_le_bytes());
        buf.extend_from_slice(&event.normalized_y.to_le_bytes());
        buf
    }

    pub fn serialize_mouse_button_event(
        header: InputPacketHeader,
        event: &MouseButtonEvent,
    ) -> Vec<u8> {
        let mut buf = Vec::with_capacity(48 + 4);
        buf.extend_from_slice(&Self::pack_header(header, InputType::MouseButtonEvent));
        buf.push(event.button as u8);
        buf.push(if event.is_down { 1 } else { 0 });
        buf.extend_from_slice(&0u16.to_le_bytes()); // reserved
        buf
    }

    pub fn serialize_mouse_wheel_event(
        header: InputPacketHeader,
        event: &MouseWheelEvent,
    ) -> Vec<u8> {
        let mut buf = Vec::with_capacity(48 + 8);
        buf.extend_from_slice(&Self::pack_header(header, InputType::MouseWheelEvent));
        buf.extend_from_slice(&event.wheel_delta_x.to_le_bytes());
        buf.extend_from_slice(&event.wheel_delta_y.to_le_bytes());
        buf
    }

    pub fn serialize_gamepad_state(header: InputPacketHeader, state: &GamepadState) -> Vec<u8> {
        let mut buf = Vec::with_capacity(48 + 20);
        buf.extend_from_slice(&Self::pack_header(header, InputType::GamepadState));
        buf.extend_from_slice(&state.index.to_le_bytes());
        buf.extend_from_slice(&state.buttons.to_le_bytes());
        buf.push(state.left_trigger);
        buf.push(state.right_trigger);
        buf.extend_from_slice(&state.thumb_lx.to_le_bytes());
        buf.extend_from_slice(&state.thumb_ly.to_le_bytes());
        buf.extend_from_slice(&state.thumb_rx.to_le_bytes());
        buf.extend_from_slice(&state.thumb_ry.to_le_bytes());
        buf.push(if state.is_connected { 1 } else { 0 });
        buf.extend_from_slice(&[0u8; 3]); // reserved
        buf
    }

    pub fn serialize_permission_message(
        header: InputPacketHeader,
        message: &PermissionMessage,
    ) -> Vec<u8> {
        let msg_type = match message.action {
            PermissionAction::Granted => InputType::PermissionGrant,
            PermissionAction::Denied | PermissionAction::Revoked => InputType::PermissionRevoke,
            _ => InputType::PermissionRequest,
        };
        let mut buf = Vec::with_capacity(48 + 4);
        buf.extend_from_slice(&Self::pack_header(header, msg_type));
        buf.push(message.action as u8);
        buf.push(message.requested_features);
        buf.extend_from_slice(&0u16.to_le_bytes()); // reserved
        buf
    }

    pub fn serialize_reset_state(header: InputPacketHeader) -> Vec<u8> {
        let mut buf = Vec::with_capacity(48);
        buf.extend_from_slice(&Self::pack_header(header, InputType::ResetState));
        buf
    }

    pub fn serialize_clipboard_event(header: InputPacketHeader, text: &str) -> Vec<u8> {
        let bytes = text.as_bytes();
        let copy_len = bytes.len().min(MAX_CLIPBOARD_TEXT_BYTES);
        let mut buf = Vec::with_capacity(48 + copy_len);
        buf.extend_from_slice(&Self::pack_header(header, InputType::ClipboardEvent));
        buf.extend_from_slice(&bytes[..copy_len]);
        buf
    }

    pub fn deserialize(bytes: &[u8]) -> Result<(InputPacketHeader, InputEvent)> {
        if bytes.len() < 48 {
            return Err(MistError::InvalidArgument(
                "Input packet too small to contain header".into(),
            ));
        }

        let mut session_id = [0u8; 32];
        session_id.copy_from_slice(&bytes[16..48]);

        let header = InputPacketHeader {
            version: u16::from_le_bytes([bytes[0], bytes[1]]),
            input_type: bytes[2],
            flags: bytes[3],
            sequence_number: u32::from_le_bytes([bytes[4], bytes[5], bytes[6], bytes[7]]),
            timestamp_ns: u64::from_le_bytes([
                bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14],
                bytes[15],
            ]),
            session_id,
        };

        let max_size = if header.input_type == InputType::ClipboardEvent as u8 {
            48 + MAX_CLIPBOARD_TEXT_BYTES
        } else {
            MAX_INPUT_PACKET_SIZE
        };

        if bytes.len() > max_size {
            return Err(MistError::InvalidArgument(
                "Input packet exceeds maximum permitted size".into(),
            ));
        }

        if header.version != INPUT_PROTOCOL_VERSION {
            return Err(MistError::InvalidArgument(
                "Unsupported input protocol version".into(),
            ));
        }

        let payload = &bytes[48..];
        let input_type = InputType::from(header.input_type);

        match input_type {
            InputType::KeyEvent => {
                if payload.len() != 12 {
                    return Err(MistError::InvalidArgument(
                        "Mismatched KeyEvent payload size".into(),
                    ));
                }
                let virtual_key =
                    u32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);
                let scan_code =
                    u32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                let action = if payload[8] == 0 {
                    KeyAction::Press
                } else {
                    KeyAction::Release
                };
                let is_extended = payload[9] != 0;
                Ok((
                    header,
                    InputEvent::Key(KeyEvent {
                        virtual_key,
                        scan_code,
                        action,
                        is_extended,
                    }),
                ))
            }

            InputType::MouseMoveEvent => {
                if payload.len() != 20 {
                    return Err(MistError::InvalidArgument(
                        "Mismatched MouseMoveEvent payload size".into(),
                    ));
                }
                let delta_x = i32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);
                let delta_y = i32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                let is_absolute = payload[8] != 0;
                let mut nx =
                    f32::from_le_bytes([payload[12], payload[13], payload[14], payload[15]]);
                let mut ny =
                    f32::from_le_bytes([payload[16], payload[17], payload[18], payload[19]]);
                if nx.is_nan() {
                    nx = 0.0;
                }
                if ny.is_nan() {
                    ny = 0.0;
                }
                Ok((
                    header,
                    InputEvent::MouseMove(MouseMoveEvent {
                        delta_x,
                        delta_y,
                        is_absolute,
                        normalized_x: nx,
                        normalized_y: ny,
                    }),
                ))
            }

            InputType::MouseButtonEvent => {
                if payload.len() != 4 {
                    return Err(MistError::InvalidArgument(
                        "Mismatched MouseButtonEvent payload size".into(),
                    ));
                }
                let btn_raw = payload[0];
                if btn_raw > MouseButton::X2 as u8 {
                    return Err(MistError::InvalidArgument("Invalid MouseButton ID".into()));
                }
                let button = MouseButton::from(btn_raw);
                let is_down = payload[1] != 0;
                Ok((
                    header,
                    InputEvent::MouseButton(MouseButtonEvent { button, is_down }),
                ))
            }

            InputType::MouseWheelEvent => {
                if payload.len() != 8 {
                    return Err(MistError::InvalidArgument(
                        "Mismatched MouseWheelEvent payload size".into(),
                    ));
                }
                let wheel_delta_x =
                    i32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);
                let wheel_delta_y =
                    i32::from_le_bytes([payload[4], payload[5], payload[6], payload[7]]);
                Ok((
                    header,
                    InputEvent::MouseWheel(MouseWheelEvent {
                        wheel_delta_x,
                        wheel_delta_y,
                    }),
                ))
            }

            InputType::GamepadState => {
                if payload.len() != 20 {
                    return Err(MistError::InvalidArgument(
                        "Mismatched GamepadState payload size".into(),
                    ));
                }
                let index = u32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);
                let buttons = u16::from_le_bytes([payload[4], payload[5]]);
                let left_trigger = payload[6];
                let right_trigger = payload[7];
                let thumb_lx = i16::from_le_bytes([payload[8], payload[9]]);
                let thumb_ly = i16::from_le_bytes([payload[10], payload[11]]);
                let thumb_rx = i16::from_le_bytes([payload[12], payload[13]]);
                let thumb_ry = i16::from_le_bytes([payload[14], payload[15]]);
                let is_connected = payload[16] != 0;

                Ok((
                    header,
                    InputEvent::Gamepad(GamepadState {
                        index,
                        buttons,
                        left_trigger,
                        right_trigger,
                        thumb_lx,
                        thumb_ly,
                        thumb_rx,
                        thumb_ry,
                        is_connected,
                    }),
                ))
            }

            InputType::PermissionRequest
            | InputType::PermissionGrant
            | InputType::PermissionRevoke => {
                if payload.len() != 4 {
                    return Err(MistError::InvalidArgument(
                        "Mismatched PermissionMessage payload size".into(),
                    ));
                }
                let action = PermissionAction::from(payload[0]);
                let requested_features = payload[1];
                Ok((
                    header,
                    InputEvent::Permission(PermissionMessage {
                        action,
                        requested_features,
                    }),
                ))
            }

            InputType::ResetState => {
                if !payload.is_empty() {
                    return Err(MistError::InvalidArgument(
                        "ResetState must have empty payload".into(),
                    ));
                }
                Ok((
                    header,
                    InputEvent::Permission(PermissionMessage {
                        action: PermissionAction::Revoked,
                        requested_features: 0,
                    }),
                ))
            }

            InputType::ClipboardEvent => {
                if payload.len() > MAX_CLIPBOARD_TEXT_BYTES {
                    return Err(MistError::InvalidArgument(
                        "Clipboard payload exceeds maximum allowed size".into(),
                    ));
                }
                let text = String::from_utf8_lossy(payload).to_string();
                let hash = compute_fnv1a(&text);
                Ok((header, InputEvent::Clipboard(ClipboardEvent { text, hash })))
            }

            InputType::Unknown | InputType::InputStateSnapshot => Err(MistError::InvalidArgument(
                "Unsupported or unknown input packet type".into(),
            )),
        }
    }
}
