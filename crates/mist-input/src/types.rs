//! Input types, structures, and binary layouts for Mist input protocol.

use serde::{Deserialize, Serialize};

pub const INPUT_PROTOCOL_VERSION: u16 = 1;
pub const MAX_SESSION_ID_LENGTH: usize = 32;
pub const MAX_INPUT_PACKET_SIZE: usize = 256;
pub const MAX_CLIPBOARD_TEXT_BYTES: usize = 1024 * 1024; // 1 MB limit

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum InputType {
    Unknown = 0,
    KeyEvent = 1,
    MouseMoveEvent = 2,
    MouseButtonEvent = 3,
    MouseWheelEvent = 4,
    GamepadState = 5,
    InputStateSnapshot = 6,
    PermissionRequest = 7,
    PermissionGrant = 8,
    PermissionRevoke = 9,
    ResetState = 10,
    ClipboardEvent = 11,
}

impl From<u8> for InputType {
    fn from(v: u8) -> Self {
        match v {
            1 => Self::KeyEvent,
            2 => Self::MouseMoveEvent,
            3 => Self::MouseButtonEvent,
            4 => Self::MouseWheelEvent,
            5 => Self::GamepadState,
            6 => Self::InputStateSnapshot,
            7 => Self::PermissionRequest,
            8 => Self::PermissionGrant,
            9 => Self::PermissionRevoke,
            10 => Self::ResetState,
            11 => Self::ClipboardEvent,
            _ => Self::Unknown,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum KeyAction {
    Press = 0,
    Release = 1,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct KeyEvent {
    pub virtual_key: u32,
    pub scan_code: u32,
    pub action: KeyAction,
    pub is_extended: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum MouseButton {
    None = 0,
    Left = 1,
    Right = 2,
    Middle = 3,
    X1 = 4,
    X2 = 5,
}

impl From<u8> for MouseButton {
    fn from(v: u8) -> Self {
        match v {
            1 => Self::Left,
            2 => Self::Right,
            3 => Self::Middle,
            4 => Self::X1,
            5 => Self::X2,
            _ => Self::None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct MouseMoveEvent {
    pub delta_x: i32,
    pub delta_y: i32,
    pub is_absolute: bool,
    pub normalized_x: f32,
    pub normalized_y: f32,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MouseButtonEvent {
    pub button: MouseButton,
    pub is_down: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MouseWheelEvent {
    pub wheel_delta_x: i32,
    pub wheel_delta_y: i32,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GamepadState {
    pub index: u32,
    pub buttons: u16,
    pub left_trigger: u8,
    pub right_trigger: u8,
    pub thumb_lx: i16,
    pub thumb_ly: i16,
    pub thumb_rx: i16,
    pub thumb_ry: i16,
    pub is_connected: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct GamepadVibration {
    pub slot_index: u32,
    pub left_motor_speed: u16,
    pub right_motor_speed: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum PermissionAction {
    Request = 0,
    Granted = 1,
    Denied = 2,
    Revoked = 3,
}

impl From<u8> for PermissionAction {
    fn from(v: u8) -> Self {
        match v {
            1 => Self::Granted,
            2 => Self::Denied,
            3 => Self::Revoked,
            _ => Self::Request,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PermissionMessage {
    pub action: PermissionAction,
    pub requested_features: u8,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ClipboardEvent {
    pub text: String,
    pub hash: u64,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum InputEvent {
    Key(KeyEvent),
    MouseMove(MouseMoveEvent),
    MouseButton(MouseButtonEvent),
    MouseWheel(MouseWheelEvent),
    Gamepad(GamepadState),
    Permission(PermissionMessage),
    Clipboard(ClipboardEvent),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub struct InputPacketHeader {
    pub version: u16,
    pub input_type: u8,
    pub flags: u8,
    pub sequence_number: u32,
    pub timestamp_ns: u64,
    pub session_id: [u8; MAX_SESSION_ID_LENGTH],
}

impl Default for InputPacketHeader {
    fn default() -> Self {
        Self {
            version: INPUT_PROTOCOL_VERSION,
            input_type: InputType::Unknown as u8,
            flags: 0,
            sequence_number: 0,
            timestamp_ns: 0,
            session_id: [0u8; MAX_SESSION_ID_LENGTH],
        }
    }
}

impl InputPacketHeader {
    pub fn new(input_type: InputType, seq: u32, timestamp_ns: u64, session_id: &str) -> Self {
        let mut sid = [0u8; MAX_SESSION_ID_LENGTH];
        let bytes = session_id.as_bytes();
        let len = bytes.len().min(MAX_SESSION_ID_LENGTH);
        sid[..len].copy_from_slice(&bytes[..len]);

        Self {
            version: INPUT_PROTOCOL_VERSION,
            input_type: input_type as u8,
            flags: 0,
            sequence_number: seq,
            timestamp_ns,
            session_id: sid,
        }
    }

    pub fn session_id_str(&self) -> String {
        let sid = self.session_id;
        let null_idx = sid.iter().position(|&b| b == 0).unwrap_or(sid.len());
        String::from_utf8_lossy(&sid[..null_idx]).to_string()
    }
}

const _: () = assert!(std::mem::size_of::<InputPacketHeader>() == 48);
