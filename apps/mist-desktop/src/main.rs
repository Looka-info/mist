//! Mist Desktop Application Entry Point
//! Native Windows game-streaming platform built with Rust and Slint.

mod app;
mod controller;

fn main() {
    mist_core::init_logging(false);
    tracing::info!("Starting Mist Desktop Application v0.2.0 (Rust + Slint)");

    if let Err(e) = app::run_app() {
        tracing::error!("Mist Desktop encountered fatal error: {}", e);
        std::process::exit(1);
    }
}
