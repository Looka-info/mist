//! Slint UI bindings, event handlers, and presentation logic for Mist Desktop.

use crate::controller::DesktopController;
use mist_domain::ConnectionState;
use mist_pairing::{RemotePairingClient, RemotePairingRequest};
use mist_session::SessionEvent;
use slint::{ComponentHandle, ModelRc, VecModel};
use std::rc::Rc;
use std::sync::Arc;
use std::time::Duration;

slint::include_modules!();

pub fn run_app() -> Result<(), slint::PlatformError> {
    let main_window = MainWindow::new()?;
    let controller = Arc::new(DesktopController::new());

    // Populate initial system info & configuration
    {
        let wifi = controller.wifi_info.lock().unwrap();
        main_window.set_wifi_adapter(wifi.adapter_name.clone().into());
        main_window.set_wifi_ssid(wifi.ssid.clone().into());
        main_window.set_wifi_signal(wifi.signal_percent as i32);
        main_window.set_wifi_band(wifi.band.clone().into());

        let hw = controller.hardware_info.lock().unwrap();
        main_window.set_gpu_name(hw.gpu_name.clone().into());
        main_window.set_nvenc_ready(hw.nvenc_supported);

        let cfg = controller.config.lock().unwrap();
        main_window.set_settings_codec(cfg.video.codec.clone().into());
        main_window.set_settings_hdr(cfg.video.hdr_enabled);
        main_window.set_settings_capture_backend(cfg.video.capture_backend.clone().into());
        main_window.set_settings_wol_mac(cfg.video.wol_mac_address.clone().into());
        main_window.set_settings_virtual_display(cfg.video.virtual_display_enabled);
        main_window
            .set_settings_virtual_display_res(cfg.video.virtual_display_resolution.clone().into());
        main_window.set_session_codec(if cfg.video.hdr_enabled {
            "AV1 (10-Bit HDR)".into()
        } else if cfg.video.codec == "hevc" {
            "HEVC (H.265)".into()
        } else if cfg.video.codec == "h264" {
            "H.264 (Universal)".into()
        } else {
            "AV1 / HEVC (Auto)".into()
        });
        main_window.set_session_display_idx(cfg.video.display_index as i32);
        main_window.set_session_relative_mouse(cfg.video.relative_mouse);
        main_window.set_host_virtual_display(cfg.video.virtual_display_enabled);
    }

    // Refresh trusted devices list
    let refresh_devices_model = {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        move || {
            if let Some(win) = win_weak.upgrade() {
                let devices = ctrl.get_trusted_devices();
                let mut items = Vec::new();
                for d in devices {
                    items.push(DeviceItemData {
                        device_id: d.id.0.into(),
                        device_name: d.name.into(),
                        last_connected: d.last_connected.into(),
                        is_trusted: d.is_trusted,
                        has_audio: d.permissions.audio,
                        has_input: d.permissions.keyboard
                            || d.permissions.mouse
                            || d.permissions.gamepad,
                    });
                }
                let model = Rc::new(VecModel::from(items));
                win.set_devices(ModelRc::from(model));
            }
        }
    };
    refresh_devices_model();

    // Navigation callback
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_nav_changed(move |tab_idx| {
            if let Some(win) = win_weak.upgrade() {
                win.set_active_tab(tab_idx);
                if tab_idx == 0 || tab_idx == 1 || tab_idx == 4 {
                    ctrl.refresh_platform_info();
                    let wifi = ctrl.wifi_info.lock().unwrap();
                    win.set_wifi_adapter(wifi.adapter_name.clone().into());
                    win.set_wifi_ssid(wifi.ssid.clone().into());
                    win.set_wifi_signal(wifi.signal_percent as i32);
                    win.set_wifi_band(wifi.band.clone().into());
                }
            }
        });
    }

    // Start Wi-Fi Share
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_start_share(move || {
            let win_weak_event = win_weak.clone();
            let win_weak_prompt = win_weak.clone();

            let event_cb = move |ev: SessionEvent| {
                let w = win_weak_event.clone();
                slint::invoke_from_event_loop(move || {
                    if let Some(win) = w.upgrade() {
                        match ev {
                            SessionEvent::StateChanged(st) => {
                                win.set_is_session_active(st == ConnectionState::Streaming);
                                if st == ConnectionState::Streaming {
                                    win.set_active_tab(7); // Switch to LiveSession
                                }
                            }
                            SessionEvent::TelemetryUpdated(t) => {
                                win.set_telem_fps(t.formatted_fps().into());
                                win.set_telem_bitrate(t.formatted_bitrate().into());
                                win.set_telem_rtt(t.formatted_rtt().into());
                                win.set_telem_jitter(t.formatted_jitter().into());
                                win.set_telem_loss(t.formatted_loss().into());
                            }
                            _ => {}
                        }
                    }
                })
                .unwrap_or(());
            };

            let prompt_cb = Arc::new(move |_req_id, client_id, client_name| {
                let w = win_weak_prompt.clone();
                slint::invoke_from_event_loop(move || {
                    if let Some(win) = w.upgrade() {
                        win.set_dialog_title("Incoming Connection Request".into());
                        win.set_dialog_message(
                            format!(
                                "Device '{}' ({}) is requesting permission to stream.",
                                client_name, client_id
                            )
                            .into(),
                        );
                        win.set_dialog_open(true);
                    }
                })
                .unwrap_or(());
            });

            match ctrl.start_sharing(event_cb, prompt_cb) {
                Ok(code) => {
                    if let Some(win) = win_weak.upgrade() {
                        win.set_is_sharing(true);
                        win.set_pairing_code(code.into());
                        win.set_remaining_seconds(300);
                        win.set_toast_message("Wi-Fi Screen Share active. Code generated.".into());
                        win.set_toast_visible(true);
                    }
                }
                Err(e) => {
                    if let Some(win) = win_weak.upgrade() {
                        win.set_toast_message(format!("Error starting share: {}", e).into());
                        win.set_toast_visible(true);
                    }
                }
            }
        });
    }

    // Stop Wi-Fi Share
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_stop_share(move || {
            ctrl.stop_sharing();
            if let Some(win) = win_weak.upgrade() {
                win.set_is_sharing(false);
                win.set_pairing_code("------".into());
                win.set_is_session_active(false);
                win.set_toast_message("Wi-Fi Screen Share stopped.".into());
                win.set_toast_visible(true);
            }
        });
    }

    // Copy code
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_copy_code(move || {
            if ctrl.copy_active_code() {
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message("Pairing code copied to clipboard.".into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    // Revoke code
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_revoke_code(move || {
            ctrl.stop_sharing();
            if let Some(win) = win_weak.upgrade() {
                win.set_is_sharing(false);
                win.set_pairing_code("------".into());
                win.set_toast_message("Pairing code revoked.".into());
                win.set_toast_visible(true);
            }
        });
    }

    // Connect with code
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_connect_requested(move |code, host_ip| {
            let code_str = code.to_string().trim().to_uppercase();
            let host_str = host_ip.to_string().trim().to_string();

            if code_str.len() != 6 {
                if let Some(win) = win_weak.upgrade() {
                    win.set_connect_error(
                        "Please enter a valid 6-character alphanumeric code.".into(),
                    );
                }
                return;
            }

            if let Some(win) = win_weak.upgrade() {
                win.set_is_connecting(true);
                win.set_connect_step(1);
                win.set_connect_error("".into());
            }

            let ctrl_thread = ctrl.clone();
            let win_thread = win_weak.clone();

            std::thread::spawn(move || {
                let req = RemotePairingRequest::new(
                    format!("client-{}", std::process::id()),
                    "Mist Client PC".to_string(),
                    code_str,
                    0x0F,
                );

                let resp = RemotePairingClient::submit_request(
                    &host_str,
                    8085,
                    req,
                    Duration::from_secs(45),
                );

                match resp {
                    Ok(r) if r.success => {
                        let w = win_thread.clone();
                        slint::invoke_from_event_loop(move || {
                            if let Some(win) = w.upgrade() {
                                win.set_connect_step(2);
                            }
                        })
                        .unwrap_or(());

                        // Start client streaming process
                        let cfg = ctrl_thread.config.lock().unwrap().clone();
                        let event_w = win_thread.clone();

                        let res = ctrl_thread.session_supervisor.lock().unwrap().start_client(
                            &r.session_id,
                            &r.auth_token,
                            &cfg,
                            0, // parent HWND can be set once window handle query is available
                            move |ev| {
                                let w2 = event_w.clone();
                                slint::invoke_from_event_loop(move || {
                                    if let Some(win) = w2.upgrade() {
                                        match ev {
                                            SessionEvent::StateChanged(
                                                ConnectionState::Connecting,
                                            ) => {
                                                win.set_connect_step(3);
                                            }
                                            SessionEvent::StateChanged(
                                                ConnectionState::Streaming,
                                            ) => {
                                                win.set_is_connecting(false);
                                                win.set_is_session_active(true);
                                                win.set_active_tab(7); // Switch to LiveSession
                                            }
                                            SessionEvent::TelemetryUpdated(t) => {
                                                win.set_telem_fps(t.formatted_fps().into());
                                                win.set_telem_bitrate(t.formatted_bitrate().into());
                                                win.set_telem_rtt(t.formatted_rtt().into());
                                                win.set_telem_jitter(t.formatted_jitter().into());
                                                win.set_telem_loss(t.formatted_loss().into());
                                            }
                                            SessionEvent::StateChanged(
                                                ConnectionState::Disconnected,
                                            ) => {
                                                win.set_is_session_active(false);
                                                win.set_is_connecting(false);
                                            }
                                            _ => {}
                                        }
                                    }
                                })
                                .unwrap_or(());
                            },
                        );

                        if let Err(e) = res {
                            let w3 = win_thread.clone();
                            slint::invoke_from_event_loop(move || {
                                if let Some(win) = w3.upgrade() {
                                    win.set_is_connecting(false);
                                    win.set_connect_error(
                                        format!("Failed to launch client: {}", e).into(),
                                    );
                                }
                            })
                            .unwrap_or(());
                        }
                    }
                    Ok(r) => {
                        let w = win_thread.clone();
                        slint::invoke_from_event_loop(move || {
                            if let Some(win) = w.upgrade() {
                                win.set_is_connecting(false);
                                win.set_connect_error(r.error_message.into());
                            }
                        })
                        .unwrap_or(());
                    }
                    Err(e) => {
                        let w = win_thread.clone();
                        slint::invoke_from_event_loop(move || {
                            if let Some(win) = w.upgrade() {
                                win.set_is_connecting(false);
                                win.set_connect_error(e.to_string().into());
                            }
                        })
                        .unwrap_or(());
                    }
                }
            });
        });
    }

    // Cancel connect
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_cancel_connect(move || {
            ctrl.session_supervisor.lock().unwrap().stop();
            if let Some(win) = win_weak.upgrade() {
                win.set_is_connecting(false);
                win.set_connect_step(0);
                win.set_connect_error("".into());
            }
        });
    }

    // Paste code
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_paste_code(move || {
            if let Some(text) = ctrl.paste_code_from_clipboard() {
                let clean: String = text
                    .chars()
                    .filter(|c| c.is_ascii_alphanumeric())
                    .take(6)
                    .collect();
                if let Some(win) = win_weak.upgrade() {
                    win.set_connect_code(clean.into());
                }
            }
        });
    }

    // Code typed - normalize uppercase
    {
        let win_weak = main_window.as_weak();
        main_window.on_code_typed(move |typed| {
            let upper: String = typed
                .to_string()
                .chars()
                .filter(|c| c.is_ascii_alphanumeric())
                .take(6)
                .collect::<String>()
                .to_uppercase();
            if let Some(win) = win_weak.upgrade() {
                if win.get_connect_code() != upper {
                    win.set_connect_code(upper.into());
                }
            }
        });
    }

    // Revoke trusted device
    {
        let ctrl = controller.clone();
        let refresh = refresh_devices_model.clone();
        main_window.on_revoke_device(move |dev_id| {
            ctrl.revoke_trusted_device(&dev_id);
            refresh();
        });
    }

    // Save settings
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_save_settings(
            move |dark,
                  high_cont,
                  fps,
                  bitrate_kbps,
                  sig_url,
                  stun,
                  audio,
                  codec,
                  hdr,
                  backend,
                  wol,
                  virt_disp,
                  virt_res| {
                {
                    let mut cfg = ctrl.config.lock().unwrap();
                    cfg.ui.dark_theme = dark;
                    cfg.ui.high_contrast = high_cont;
                    cfg.video.fps = fps as u32;
                    cfg.video.bitrate_kbps = bitrate_kbps as u32;
                    cfg.network.signaling_url = sig_url.to_string();
                    cfg.network.stun_server = stun.to_string();
                    cfg.audio.enabled = audio;
                    cfg.video.codec = codec.to_string();
                    cfg.video.hdr_enabled = hdr;
                    cfg.video.capture_backend = backend.to_string();
                    cfg.video.wol_mac_address = wol.to_string();
                    cfg.video.virtual_display_enabled = virt_disp;
                    cfg.video.virtual_display_resolution = virt_res.to_string();
                    let _ = cfg.save();
                }

                // In-flight reconfiguration update via IPC
                ctrl.session_supervisor
                    .lock()
                    .unwrap()
                    .update_config(bitrate_kbps as u32, fps as u32);

                if let Some(win) = win_weak.upgrade() {
                    win.set_host_virtual_display(virt_disp);
                    win.set_toast_message("Settings saved and applied.".into());
                    win.set_toast_visible(true);
                }
            },
        );
    }

    // Wake-on-LAN dispatch
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_send_wol(move |mac| {
            let res = ctrl.send_wol(&mac);
            if let Some(win) = win_weak.upgrade() {
                match res {
                    Ok(()) => {
                        win.set_toast_message(
                            format!("Magic packet broadcasted to {}", mac).into(),
                        );
                    }
                    Err(e) => {
                        win.set_toast_message(format!("WoL error: {}", e).into());
                    }
                }
                win.set_toast_visible(true);
            }
        });
    }

    // Multi-monitor cycle
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_cycle_display(move || {
            let next_idx = ctrl.cycle_display();
            if let Some(win) = win_weak.upgrade() {
                win.set_session_display_idx(next_idx as i32);
                win.set_toast_message(format!("Switched to Display {}", next_idx).into());
                win.set_toast_visible(true);
            }
        });
    }

    // Toggle relative mouse confinement
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_toggle_relative_mouse(move || {
            let val = ctrl.toggle_relative_mouse();
            if let Some(win) = win_weak.upgrade() {
                win.set_session_relative_mouse(val);
                win.set_toast_message(if val {
                    "Relative Mouse Confined (Esc to free)".into()
                } else {
                    "Mouse Unconfined".into()
                });
                win.set_toast_visible(true);
            }
        });
    }

    // Toggle headless virtual display
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_toggle_virtual_display(move || {
            let val = ctrl.toggle_virtual_display();
            if let Some(win) = win_weak.upgrade() {
                win.set_settings_virtual_display(val);
                win.set_host_virtual_display(val);
                win.set_toast_message(if val {
                    "IddCx Virtual Display Activated".into()
                } else {
                    "Virtual Display Detached".into()
                });
                win.set_toast_visible(true);
            }
        });
    }

    // Dialog decisions
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_devices_model.clone();
        main_window.on_dialog_confirmed(move || {
            ctrl.approve_pending_request();
            if let Some(win) = win_weak.upgrade() {
                win.set_dialog_open(false);
                win.set_toast_message("Device approved and authorized.".into());
                win.set_toast_visible(true);
            }
            refresh();
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_dialog_cancelled(move || {
            ctrl.deny_pending_request();
            if let Some(win) = win_weak.upgrade() {
                win.set_dialog_open(false);
                win.set_toast_message("Device connection request denied.".into());
                win.set_toast_visible(true);
            }
        });
    }

    // Disconnect stream
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_disconnect_stream(move || {
            ctrl.session_supervisor.lock().unwrap().stop();
            if let Some(win) = win_weak.upgrade() {
                win.set_is_session_active(false);
                win.set_active_tab(0); // Return home
                win.set_toast_message("Streaming session ended.".into());
                win.set_toast_visible(true);
            }
        });
    }

    // Fullscreen
    {
        let win_weak = main_window.as_weak();
        main_window.on_toggle_fullscreen(move || {
            if let Some(win) = win_weak.upgrade() {
                win.window().set_fullscreen(!win.window().is_fullscreen());
            }
        });
    }

    // Background timer (1 Hz): updates countdown, cleans toast, checks lockout
    let timer = slint::Timer::default();
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let mut toast_counter = 0;

        timer.start(
            slint::TimerMode::Repeated,
            Duration::from_secs(1),
            move || {
                if let Some(win) = win_weak.upgrade() {
                    let pm = ctrl.pairing_manager.lock().unwrap();

                    // Code expiration countdown
                    let rem = pm.active_code_remaining_secs() as i32;
                    win.set_remaining_seconds(rem);
                    if rem == 0 && win.get_is_sharing() {
                        win.set_pairing_code("EXPIRED".into());
                    }

                    // Lockout countdown
                    let is_lock = pm.is_locked_out();
                    win.set_is_locked_out(is_lock);
                    win.set_lockout_seconds(pm.lockout_remaining_secs() as i32);

                    // Auto-hide toast after 3 seconds
                    if win.get_toast_visible() {
                        toast_counter += 1;
                        if toast_counter >= 3 {
                            win.set_toast_visible(false);
                            toast_counter = 0;
                        }
                    } else {
                        toast_counter = 0;
                    }
                }
            },
        );
    }

    main_window.run()
}
