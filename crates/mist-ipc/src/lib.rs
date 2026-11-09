//! Native Windows Named Pipes IPC protocol matching Mist's binary frame specification.

use mist_core::{MistError, Result};
use std::ffi::OsStr;
use std::os::windows::ffi::OsStrExt;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

pub type HANDLE = *mut std::ffi::c_void;
pub const INVALID_HANDLE_VALUE: HANDLE = -1isize as HANDLE;
pub const PIPE_ACCESS_DUPLEX: u32 = 0x00000003;
pub const PIPE_TYPE_MESSAGE: u32 = 0x00000004;
pub const PIPE_READMODE_MESSAGE: u32 = 0x00000002;
pub const PIPE_WAIT: u32 = 0x00000000;

#[link(name = "kernel32")]
extern "system" {
    fn CreateNamedPipeW(
        lpName: *const u16,
        dwOpenMode: u32,
        dwPipeMode: u32,
        nMaxInstances: u32,
        nOutBufferSize: u32,
        nInBufferSize: u32,
        nDefaultTimeOut: u32,
        lpSecurityAttributes: *const std::ffi::c_void,
    ) -> HANDLE;

    fn ConnectNamedPipe(hNamedPipe: HANDLE, lpOverlapped: *mut std::ffi::c_void) -> i32;
    fn DisconnectNamedPipe(hNamedPipe: HANDLE) -> i32;

    fn ReadFile(
        hFile: HANDLE,
        lpBuffer: *mut std::ffi::c_void,
        nNumberOfBytesToRead: u32,
        lpNumberOfBytesRead: *mut u32,
        lpOverlapped: *mut std::ffi::c_void,
    ) -> i32;

    fn WriteFile(
        hFile: HANDLE,
        lpBuffer: *const std::ffi::c_void,
        nNumberOfBytesToWrite: u32,
        lpNumberOfBytesWritten: *mut u32,
        lpOverlapped: *mut std::ffi::c_void,
    ) -> i32;

    fn CloseHandle(hObject: HANDLE) -> i32;
    fn GetLastError() -> u32;
}

pub const IPC_MAGIC: u32 = 0x4D495354; // 'MIST'
pub const IPC_VERSION: u16 = 1;
pub const IPC_HEADER_SIZE: usize = 20;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u16)]
pub enum MessageType {
    Unknown = 0,
    Heartbeat = 1,
    ProcessInitialized = 2,
    SignalingConnected = 3,
    IceNegotiationStarted = 4,
    IceConnected = 5,
    HostApprovalReceived = 6,
    VideoTrackReceived = 7,
    DecoderInitialized = 8,
    FirstFramePresented = 9,
    AudioPlaybackActive = 10,
    TelemetryUpdate = 11,
    UpdateConfig = 12,
    ConfigApplied = 13,
    ConnectionLost = 14,
    StopStreaming = 15,
    StreamingStopped = 16,
    WindowHandle = 17,
    SetViewport = 18,
    StreamingActive = 19,
}

impl From<u16> for MessageType {
    fn from(val: u16) -> Self {
        match val {
            1 => Self::Heartbeat,
            2 => Self::ProcessInitialized,
            3 => Self::SignalingConnected,
            4 => Self::IceNegotiationStarted,
            5 => Self::IceConnected,
            6 => Self::HostApprovalReceived,
            7 => Self::VideoTrackReceived,
            8 => Self::DecoderInitialized,
            9 => Self::FirstFramePresented,
            10 => Self::AudioPlaybackActive,
            11 => Self::TelemetryUpdate,
            12 => Self::UpdateConfig,
            13 => Self::ConfigApplied,
            14 => Self::ConnectionLost,
            15 => Self::StopStreaming,
            16 => Self::StreamingStopped,
            17 => Self::WindowHandle,
            18 => Self::SetViewport,
            19 => Self::StreamingActive,
            _ => Self::Unknown,
        }
    }
}

#[derive(Debug, Clone)]
pub struct IpcMessage {
    pub msg_type: MessageType,
    pub timestamp_ns: u64,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone, Copy, Default)]
#[repr(C)]
pub struct IpcTelemetryPayload {
    pub bitrate_kbps: u32,
    pub rtt_ms: f32,
    pub jitter_ms: f32,
    pub fps: f32,
    pub packet_loss_percent: f32,
    pub frames_decoded: u32,
    pub frames_dropped: u32,
    pub stats_timestamp_ns: u64,
}

#[derive(Debug, Clone, Copy, Default)]
#[repr(C)]
pub struct IpcConfigPayload {
    pub bitrate_kbps: u32,
    pub fps: u32,
    pub width: u32,
    pub height: u32,
}

#[derive(Debug, Clone, Copy, Default)]
#[repr(C)]
pub struct IpcWindowPayload {
    pub hwnd: u64,
    pub width: u32,
    pub height: u32,
}

#[derive(Debug, Clone, Copy, Default)]
#[repr(C)]
pub struct IpcViewportPayload {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
}

impl IpcMessage {
    pub fn new(msg_type: MessageType) -> Self {
        Self {
            msg_type,
            timestamp_ns: mist_core::now_nanos(),
            payload: Vec::new(),
        }
    }

    pub fn with_payload<T: Copy>(msg_type: MessageType, payload: &T) -> Self {
        let size = std::mem::size_of::<T>();
        let mut bytes = vec![0u8; size];
        unsafe {
            std::ptr::copy_nonoverlapping(
                payload as *const T as *const u8,
                bytes.as_mut_ptr(),
                size,
            );
        }
        Self {
            msg_type,
            timestamp_ns: mist_core::now_nanos(),
            payload: bytes,
        }
    }

    pub fn get_payload<T: Copy>(&self) -> Option<T> {
        if self.payload.len() != std::mem::size_of::<T>() {
            return None;
        }
        let mut item = std::mem::MaybeUninit::<T>::uninit();
        unsafe {
            std::ptr::copy_nonoverlapping(
                self.payload.as_ptr(),
                item.as_mut_ptr() as *mut u8,
                std::mem::size_of::<T>(),
            );
            Some(item.assume_init())
        }
    }

    pub fn serialize(&self) -> Vec<u8> {
        let mut buffer = Vec::with_capacity(IPC_HEADER_SIZE + self.payload.len());
        buffer.extend_from_slice(&IPC_MAGIC.to_le_bytes());
        buffer.extend_from_slice(&IPC_VERSION.to_le_bytes());
        buffer.extend_from_slice(&(self.msg_type as u16).to_le_bytes());
        buffer.extend_from_slice(&(self.payload.len() as u32).to_le_bytes());
        buffer.extend_from_slice(&self.timestamp_ns.to_le_bytes());
        buffer.extend_from_slice(&self.payload);
        buffer
    }

    pub fn deserialize(bytes: &[u8]) -> Option<Self> {
        if bytes.len() < IPC_HEADER_SIZE {
            return None;
        }
        let magic = u32::from_le_bytes(bytes[0..4].try_into().unwrap());
        if magic != IPC_MAGIC {
            return None;
        }
        let version = u16::from_le_bytes(bytes[4..6].try_into().unwrap());
        if version != IPC_VERSION {
            return None;
        }
        let msg_type_raw = u16::from_le_bytes(bytes[6..8].try_into().unwrap());
        let payload_len = u32::from_le_bytes(bytes[8..12].try_into().unwrap()) as usize;
        let timestamp_ns = u64::from_le_bytes(bytes[12..20].try_into().unwrap());

        if bytes.len() < IPC_HEADER_SIZE + payload_len {
            return None;
        }

        let payload = bytes[IPC_HEADER_SIZE..IPC_HEADER_SIZE + payload_len].to_vec();

        Some(Self {
            msg_type: MessageType::from(msg_type_raw),
            timestamp_ns,
            payload,
        })
    }
}

pub struct NamedPipeServer {
    pipe_handle: Arc<Mutex<isize>>,
    is_running: Arc<AtomicBool>,
}

impl Default for NamedPipeServer {
    fn default() -> Self {
        Self {
            pipe_handle: Arc::new(Mutex::new(INVALID_HANDLE_VALUE as isize)),
            is_running: Arc::new(AtomicBool::new(false)),
        }
    }
}

impl NamedPipeServer {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn start<F>(&mut self, pipe_name: &str, on_message: F) -> Result<()>
    where
        F: Fn(IpcMessage) + Send + Sync + 'static,
    {
        let wide_name: Vec<u16> = OsStr::new(pipe_name).encode_wide().chain(Some(0)).collect();
        let handle = unsafe {
            CreateNamedPipeW(
                wide_name.as_ptr(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,
                65536,
                65536,
                0,
                std::ptr::null(),
            )
        };

        if handle == INVALID_HANDLE_VALUE {
            return Err(MistError::Ipc(format!("Failed to create named pipe {}", pipe_name)));
        }

        *self.pipe_handle.lock().unwrap() = handle as isize;
        self.is_running.store(true, Ordering::SeqCst);

        let pipe_arc = self.pipe_handle.clone();
        let running_arc = self.is_running.clone();
        let cb = Arc::new(on_message);

        std::thread::spawn(move || {
            let h = *pipe_arc.lock().unwrap() as HANDLE;
            let connected = unsafe { ConnectNamedPipe(h, std::ptr::null_mut()) };
            let err = unsafe { GetLastError() };
            if connected == 0 && err != 535 { // ERROR_PIPE_CONNECTED
                tracing::warn!("Named pipe client connection failed, code: {}", err);
                return;
            }

            tracing::info!("Named pipe client connected successfully.");

            let mut buffer = vec![0u8; 65536];
            while running_arc.load(Ordering::SeqCst) {
                let mut bytes_read = 0u32;
                let success = unsafe {
                    ReadFile(
                        h,
                        buffer.as_mut_ptr() as _,
                        buffer.len() as u32,
                        &mut bytes_read,
                        std::ptr::null_mut(),
                    )
                };

                if success != 0 && bytes_read >= IPC_HEADER_SIZE as u32 {
                    if let Some(msg) = IpcMessage::deserialize(&buffer[0..bytes_read as usize]) {
                        cb(msg);
                    }
                } else {
                    let err = unsafe { GetLastError() };
                    if err == 109 { // ERROR_BROKEN_PIPE
                        tracing::info!("Named pipe client disconnected.");
                        break;
                    }
                    std::thread::sleep(Duration::from_millis(10));
                }
            }
        });

        Ok(())
    }

    pub fn send_message(&self, message: &IpcMessage) -> bool {
        let h = *self.pipe_handle.lock().unwrap() as HANDLE;
        if h == INVALID_HANDLE_VALUE {
            return false;
        }

        let bytes = message.serialize();
        let mut bytes_written = 0u32;
        let success = unsafe {
            WriteFile(
                h,
                bytes.as_ptr() as _,
                bytes.len() as u32,
                &mut bytes_written,
                std::ptr::null_mut(),
            )
        };

        success != 0 && bytes_written == bytes.len() as u32
    }

    pub fn stop(&mut self) {
        self.is_running.store(false, Ordering::SeqCst);
        let mut guard = self.pipe_handle.lock().unwrap();
        if *guard != INVALID_HANDLE_VALUE as isize {
            unsafe {
                DisconnectNamedPipe(*guard as HANDLE);
                CloseHandle(*guard as HANDLE);
            }
            *guard = INVALID_HANDLE_VALUE as isize;
        }
    }
}

impl Drop for NamedPipeServer {
    fn drop(&mut self) {
        self.stop();
    }
}
