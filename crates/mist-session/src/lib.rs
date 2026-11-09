//! Session management, process supervision, and IPC state bridge for Mist.

use mist_config::AppConfig;
use mist_core::{MistError, Result};
use mist_domain::ConnectionState;
use mist_ipc::{
    IpcConfigPayload, IpcMessage, IpcTelemetryPayload, IpcViewportPayload, IpcWindowPayload,
    MessageType, NamedPipeServer,
};
use mist_telemetry::SessionTelemetry;
use std::path::PathBuf;
use std::process::{Child, Command};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

#[derive(Debug, Clone)]
pub enum SessionEvent {
    StateChanged(ConnectionState),
    TelemetryUpdated(SessionTelemetry),
    ClientWindowReady(u64),
    Error(String),
}

pub struct SessionSupervisor {
    child_process: Option<Child>,
    ipc_server: Option<NamedPipeServer>,
    current_state: Arc<Mutex<ConnectionState>>,
    latest_telemetry: Arc<Mutex<SessionTelemetry>>,
    client_hwnd: Arc<Mutex<u64>>,
    is_streaming: Arc<AtomicBool>,
}

impl Default for SessionSupervisor {
    fn default() -> Self {
        Self {
            child_process: None,
            ipc_server: None,
            current_state: Arc::new(Mutex::new(ConnectionState::Idle)),
            latest_telemetry: Arc::new(Mutex::new(SessionTelemetry::default())),
            client_hwnd: Arc::new(Mutex::new(0)),
            is_streaming: Arc::new(AtomicBool::new(false)),
        }
    }
}

impl SessionSupervisor {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn current_state(&self) -> ConnectionState {
        *self.current_state.lock().unwrap()
    }

    pub fn latest_telemetry(&self) -> SessionTelemetry {
        self.latest_telemetry.lock().unwrap().clone()
    }

    pub fn client_hwnd(&self) -> u64 {
        *self.client_hwnd.lock().unwrap()
    }

    pub fn is_streaming(&self) -> bool {
        self.is_streaming.load(Ordering::SeqCst)
    }

    fn find_binary(name: &str) -> Option<PathBuf> {
        let candidates = [
            PathBuf::from(format!("bin/{}.exe", name)),
            PathBuf::from(format!("build/apps/{}/Release/{}.exe", name, name)),
            PathBuf::from(format!("build/apps/{}/{}.exe", name, name)),
            PathBuf::from(format!("{}.exe", name)),
        ];

        for c in &candidates {
            if c.exists() {
                return Some(c.clone());
            }
        }
        None
    }

    pub fn start_host<F>(&mut self, session_id: &str, config: &AppConfig, event_cb: F) -> Result<()>
    where
        F: Fn(SessionEvent) + Send + Sync + 'static,
    {
        self.stop();

        let host_bin = Self::find_binary("mist_host").ok_or_else(|| {
            MistError::Session(
                "mist_host.exe binary not found. Please build the project first.".to_string(),
            )
        })?;

        let pid = std::process::id();
        let pipe_name = format!(r"\\.\pipe\mist_ipc_host_{}", pid);

        let state_arc = self.current_state.clone();
        let telem_arc = self.latest_telemetry.clone();
        let is_stream_arc = self.is_streaming.clone();
        let cb_arc = Arc::new(event_cb);

        let mut ipc = NamedPipeServer::new();
        let cb_clone = cb_arc.clone();
        let state_clone = state_arc.clone();
        let telem_clone = telem_arc.clone();
        let is_str_clone = is_stream_arc.clone();

        ipc.start(&pipe_name, move |msg| match msg.msg_type {
            MessageType::ProcessInitialized => {
                *state_clone.lock().unwrap() = ConnectionState::Initializing;
                cb_clone(SessionEvent::StateChanged(ConnectionState::Initializing));
            }
            MessageType::SignalingConnected => {
                *state_clone.lock().unwrap() = ConnectionState::Connecting;
                cb_clone(SessionEvent::StateChanged(ConnectionState::Connecting));
            }
            MessageType::IceConnected => {
                *state_clone.lock().unwrap() = ConnectionState::Streaming;
                is_str_clone.store(true, Ordering::SeqCst);
                cb_clone(SessionEvent::StateChanged(ConnectionState::Streaming));
            }
            MessageType::TelemetryUpdate => {
                if let Some(p) = msg.get_payload::<IpcTelemetryPayload>() {
                    let t = SessionTelemetry {
                        fps: p.fps,
                        bitrate_kbps: p.bitrate_kbps,
                        rtt_ms: p.rtt_ms,
                        jitter_ms: p.jitter_ms,
                        packet_loss_percent: p.packet_loss_percent,
                        frames_decoded: p.frames_decoded,
                        frames_dropped: p.frames_dropped,
                        timestamp_ns: p.stats_timestamp_ns,
                        source: "webrtc_media".to_string(),
                    };
                    *telem_clone.lock().unwrap() = t.clone();
                    cb_clone(SessionEvent::TelemetryUpdated(t));
                }
            }
            MessageType::ConnectionLost => {
                *state_clone.lock().unwrap() = ConnectionState::Disconnected;
                is_str_clone.store(false, Ordering::SeqCst);
                cb_clone(SessionEvent::StateChanged(ConnectionState::Disconnected));
            }
            MessageType::StreamingStopped => {
                *state_clone.lock().unwrap() = ConnectionState::Idle;
                is_str_clone.store(false, Ordering::SeqCst);
                cb_clone(SessionEvent::StateChanged(ConnectionState::Idle));
            }
            _ => {}
        })?;

        let mut cmd = Command::new(host_bin);
        cmd.args([
            "--stream-live",
            "--session-id",
            session_id,
            "--signaling",
            &config.network.signaling_url,
            "--bitrate",
            &config.video.bitrate_kbps.to_string(),
            "--fps",
            &config.video.fps.to_string(),
            "--ipc-pipe",
            &pipe_name,
        ]);

        let child = cmd
            .spawn()
            .map_err(|e| MistError::Session(format!("Failed to spawn mist_host: {}", e)))?;

        self.child_process = Some(child);
        self.ipc_server = Some(ipc);
        *self.current_state.lock().unwrap() = ConnectionState::Initializing;
        cb_arc(SessionEvent::StateChanged(ConnectionState::Initializing));

        Ok(())
    }

    pub fn start_client<F>(
        &mut self,
        session_id: &str,
        auth_token: &str,
        config: &AppConfig,
        parent_hwnd: u64,
        event_cb: F,
    ) -> Result<()>
    where
        F: Fn(SessionEvent) + Send + Sync + 'static,
    {
        self.stop();

        let client_bin = Self::find_binary("mist_client").ok_or_else(|| {
            MistError::Session(
                "mist_client.exe binary not found. Please build the project first.".to_string(),
            )
        })?;

        let pid = std::process::id();
        let pipe_name = format!(r"\\.\pipe\mist_ipc_client_{}", pid);

        let state_arc = self.current_state.clone();
        let telem_arc = self.latest_telemetry.clone();
        let is_stream_arc = self.is_streaming.clone();
        let hwnd_arc = self.client_hwnd.clone();
        let cb_arc = Arc::new(event_cb);

        let mut ipc = NamedPipeServer::new();
        let cb_clone = cb_arc.clone();
        let state_clone = state_arc.clone();
        let telem_clone = telem_arc.clone();
        let is_str_clone = is_stream_arc.clone();
        let hwnd_clone = hwnd_arc.clone();

        ipc.start(&pipe_name, move |msg| match msg.msg_type {
            MessageType::ProcessInitialized => {
                *state_clone.lock().unwrap() = ConnectionState::Initializing;
                cb_clone(SessionEvent::StateChanged(ConnectionState::Initializing));
            }
            MessageType::SignalingConnected => {
                *state_clone.lock().unwrap() = ConnectionState::Connecting;
                cb_clone(SessionEvent::StateChanged(ConnectionState::Connecting));
            }
            MessageType::WindowHandle => {
                if let Some(p) = msg.get_payload::<IpcWindowPayload>() {
                    *hwnd_clone.lock().unwrap() = p.hwnd;
                    cb_clone(SessionEvent::ClientWindowReady(p.hwnd));
                }
            }
            MessageType::FirstFramePresented => {
                *state_clone.lock().unwrap() = ConnectionState::Streaming;
                is_str_clone.store(true, Ordering::SeqCst);
                cb_clone(SessionEvent::StateChanged(ConnectionState::Streaming));
            }
            MessageType::TelemetryUpdate => {
                if let Some(p) = msg.get_payload::<IpcTelemetryPayload>() {
                    let t = SessionTelemetry {
                        fps: p.fps,
                        bitrate_kbps: p.bitrate_kbps,
                        rtt_ms: p.rtt_ms,
                        jitter_ms: p.jitter_ms,
                        packet_loss_percent: p.packet_loss_percent,
                        frames_decoded: p.frames_decoded,
                        frames_dropped: p.frames_dropped,
                        timestamp_ns: p.stats_timestamp_ns,
                        source: "webrtc_media".to_string(),
                    };
                    *telem_clone.lock().unwrap() = t.clone();
                    cb_clone(SessionEvent::TelemetryUpdated(t));
                }
            }
            MessageType::ConnectionLost => {
                *state_clone.lock().unwrap() = ConnectionState::Disconnected;
                is_str_clone.store(false, Ordering::SeqCst);
                cb_clone(SessionEvent::StateChanged(ConnectionState::Disconnected));
            }
            MessageType::StreamingStopped => {
                *state_clone.lock().unwrap() = ConnectionState::Idle;
                is_str_clone.store(false, Ordering::SeqCst);
                cb_clone(SessionEvent::StateChanged(ConnectionState::Idle));
            }
            _ => {}
        })?;

        let mut cmd = Command::new(client_bin);
        cmd.args([
            "--stream-receive",
            "--session-id",
            session_id,
            "--auth-token",
            auth_token,
            "--signaling",
            &config.network.signaling_url,
            "--ipc-pipe",
            &pipe_name,
            "--parent-hwnd",
            &parent_hwnd.to_string(),
        ]);

        let child = cmd
            .spawn()
            .map_err(|e| MistError::Session(format!("Failed to spawn mist_client: {}", e)))?;

        self.child_process = Some(child);
        self.ipc_server = Some(ipc);
        *self.current_state.lock().unwrap() = ConnectionState::Initializing;
        cb_arc(SessionEvent::StateChanged(ConnectionState::Initializing));

        Ok(())
    }

    pub fn update_config(&self, bitrate_kbps: u32, fps: u32) -> bool {
        if let Some(ref ipc) = self.ipc_server {
            let p = IpcConfigPayload {
                bitrate_kbps,
                fps,
                width: 1920,
                height: 1080,
            };
            let msg = IpcMessage::with_payload(MessageType::UpdateConfig, &p);
            ipc.send_message(&msg)
        } else {
            false
        }
    }

    pub fn set_viewport(&self, x: i32, y: i32, width: u32, height: u32) -> bool {
        if let Some(ref ipc) = self.ipc_server {
            let p = IpcViewportPayload {
                x,
                y,
                width,
                height,
            };
            let msg = IpcMessage::with_payload(MessageType::SetViewport, &p);
            ipc.send_message(&msg)
        } else {
            false
        }
    }

    pub fn stop(&mut self) {
        if let Some(ref ipc) = self.ipc_server {
            let stop_msg = IpcMessage::new(MessageType::StopStreaming);
            ipc.send_message(&stop_msg);
        }

        if let Some(mut child) = self.child_process.take() {
            let _ = child.kill();
            let _ = child.wait();
        }

        if let Some(mut ipc) = self.ipc_server.take() {
            ipc.stop();
        }

        *self.current_state.lock().unwrap() = ConnectionState::Idle;
        self.is_streaming.store(false, Ordering::SeqCst);
    }
}

impl Drop for SessionSupervisor {
    fn drop(&mut self) {
        self.stop();
    }
}
