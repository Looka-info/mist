//! Core types, error definitions, and logging initialization for Mist.

use std::sync::atomic::{AtomicBool, Ordering};
use thiserror::Error;

#[derive(Error, Debug, Clone, PartialEq, Eq)]
pub enum MistError {
    #[error("I/O error: {0}")]
    Io(String),

    #[error("Configuration error: {0}")]
    Config(String),

    #[error("Pairing error: {0}")]
    Pairing(String),

    #[error("IPC error: {0}")]
    Ipc(String),

    #[error("Session error: {0}")]
    Session(String),

    #[error("Platform error: {0}")]
    Platform(String),

    #[error("Timeout occurred: {0}")]
    Timeout(String),

    #[error("Invalid argument: {0}")]
    InvalidArgument(String),

    #[error("Internal error: {0}")]
    Internal(String),
}

pub type Result<T> = std::result::Result<T, MistError>;

static LOG_INITIALIZED: AtomicBool = AtomicBool::new(false);

/// Initializes the global tracing subscriber if not already initialized.
pub fn init_logging(verbose: bool) {
    if LOG_INITIALIZED.swap(true, Ordering::SeqCst) {
        return;
    }

    let filter = if verbose {
        "mist=debug,info"
    } else {
        "mist=info,warn,error"
    };

    let env_filter = tracing_subscriber::EnvFilter::try_from_default_env()
        .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new(filter));

    let subscriber = tracing_subscriber::fmt()
        .with_env_filter(env_filter)
        .with_target(false)
        .with_thread_ids(false)
        .with_file(false)
        .with_line_number(false)
        .finish();

    let _ = tracing::subscriber::set_global_default(subscriber);
}

/// Returns current monotonic timestamp in nanoseconds.
pub fn now_nanos() -> u64 {
    use std::time::SystemTime;
    SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .map(|d| d.as_nanos() as u64)
        .unwrap_or(0)
}
