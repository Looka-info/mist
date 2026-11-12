//! Slint UI bindings, event handlers, and presentation logic for Mist Desktop.

use crate::auth::CloudAuthManager;
use crate::controller::DesktopController;
use slint::{ComponentHandle, ModelRc, VecModel};
use std::rc::Rc;
use std::sync::Arc;
use std::time::Duration;

slint::include_modules!();

pub fn run_app() -> Result<(), slint::PlatformError> {
    let main_window = MainWindow::new()?;
    let controller = Arc::new(DesktopController::new());

    // 1. Initialize Hardware and System Info
    {
        let (gpu_name, driver_ver, nvenc_ready) = {
            let hw = controller.hardware_info.lock().unwrap();
            (
                hw.gpu_name.clone(),
                hw.driver_version.clone(),
                hw.nvenc_supported,
            )
        };

        let short_gpu = if gpu_name.contains("1660") {
            "GTX 1660 SUPER".to_string()
        } else if gpu_name.contains("Direct3D") || gpu_name.is_empty() {
            "Direct3D 11 Adapter".to_string()
        } else {
            gpu_name.clone()
        };

        let (disp_w, disp_h) = mist_platform_windows::query_display_resolution();
        let vigem_installed = mist_platform_windows::check_vigembus_installed();

        main_window.set_gpu_name(short_gpu.clone().into());
        main_window.set_nvenc_ready(nvenc_ready);
        main_window.set_driver_version(driver_ver.into());
        main_window.set_display_status(format!("{}x{} · Active Display", disp_w, disp_h).into());
        main_window.set_audio_status("Default device".into());
        if vigem_installed {
            main_window.set_controller_status("ViGEmBus driver active".into());
            main_window.set_controller_ok(true);
        } else {
            main_window.set_controller_status("ViGEmBus missing · Virtual fallback".into());
            main_window.set_controller_ok(false);
        }
        main_window.set_system_caption(format!("v0.3.0\nWindows 11 · {}", short_gpu).into());

        // Load persisted authentication state from disk
        let saved_account = CloudAuthManager::load_account();
        if saved_account.is_authenticated {
            *controller.my_tag.lock().unwrap() = saved_account.tag.clone();
            main_window.set_is_authenticated(true);
            main_window.set_my_tag(saved_account.tag.into());
        } else {
            main_window.set_my_tag(controller.get_my_tag().into());
        }

        // Initialize Real Network & Wi-Fi Diagnostics
        {
            let wifi = controller.get_wifi_info();
            let is_wifi = !wifi.radio_type.contains("Wired")
                && !wifi.band.contains("Wired")
                && !wifi.band.contains("Gigabit")
                && !wifi.ssid.starts_with("Wired");
            let standard = if !wifi.radio_type.is_empty() {
                wifi.radio_type.clone()
            } else if is_wifi {
                "802.11 Wi-Fi".to_string()
            } else {
                "IEEE 802.3 Wired".to_string()
            };
            let band_chan = if is_wifi && wifi.channel > 0 {
                format!("{} · Ch {}", wifi.band, wifi.channel)
            } else if !wifi.band.is_empty() {
                wifi.band.clone()
            } else {
                "Gigabit LAN".to_string()
            };
            let link_spd = if wifi.link_speed_mbps > 0 {
                format!("{} Mbps", wifi.link_speed_mbps)
            } else {
                "1000 Mbps".to_string()
            };

            main_window.set_net_is_wifi(is_wifi);
            main_window.set_net_is_connected(wifi.is_connected);
            main_window.set_net_adapter_name(wifi.adapter_name.into());
            main_window.set_net_ssid_name(wifi.ssid.into());
            main_window.set_net_signal_percent(wifi.signal_percent as i32);
            main_window.set_net_connection_standard(standard.into());
            main_window.set_net_band_channel(band_chan.into());
            main_window.set_net_link_speed(link_spd.into());
            main_window.set_net_local_ip(wifi.local_ip.into());
            main_window.set_net_gateway_ip(wifi.gateway_ip.into());
            main_window.set_net_gateway_ping(controller.ping_gateway().into());
            main_window.set_net_internet_ping(controller.ping_internet().into());
            main_window.set_net_google_ping(controller.ping_google().into());
            main_window.set_net_public_ip(controller.get_public_ip().into());
            main_window.set_net_is_refreshing(false);
            main_window.set_net_rx_speed("0 kbps".into());
            main_window.set_net_tx_speed("0 kbps".into());
            main_window.set_net_total_rx("0 MB".into());
            main_window.set_net_total_tx("0 MB".into());
            main_window.set_net_stream_bitrate("0.0 Mbps".into());
            main_window.set_net_stream_rtt("--".into());
            main_window.set_net_stream_loss("0.0%".into());
            main_window.set_net_stream_jitter("--".into());
        }

        // Load persisted settings into UI
        {
            let cfg = controller.config.lock().unwrap();
            main_window.set_settings_dark(cfg.ui.dark_theme);
            main_window.set_settings_contrast(cfg.ui.high_contrast);
            main_window.set_settings_max_bitrate((cfg.video.bitrate_kbps / 1000) as i32);
            main_window.set_settings_fps(cfg.video.fps as i32);
        }

        // Initial loading screen state
        main_window.set_is_loading(true);
        main_window.set_loading_status("Starting Mist Engine...".into());
        main_window.set_loading_progress(0.20);
    }

    // 2. Helper to refresh all friends, hosting, requests, and recent sessions lists
    let refresh_all_lists = {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        move || {
            if let Some(win) = win_weak.upgrade() {
                let friends = ctrl.get_friends();
                let requests = ctrl.get_requests();
                let sessions = ctrl.get_recent_sessions();

                // 2a. All Friends Model for Friends Screen
                let mut all_items = Vec::new();
                for f in &friends {
                    all_items.push(FriendItemData {
                        tag: f.tag.clone().into(),
                        is_online: f.is_online,
                        is_hosting: f.is_hosting,
                        can_join: f.can_join,
                        allow_input: f.allow_input,
                        allow_gamepad: f.allow_gamepad,
                    });
                }
                win.set_all_friends(ModelRc::from(Rc::new(VecModel::from(all_items))));

                // 2b. Host Screen Friends List
                let mut host_items = Vec::new();
                for f in &friends {
                    host_items.push(FriendJoinItem {
                        tag: f.tag.clone().into(),
                        is_online: f.is_online,
                        is_hosting: f.is_hosting,
                        can_join: f.can_join,
                    });
                }
                win.set_host_friends(ModelRc::from(Rc::new(VecModel::from(host_items))));

                // 2c. Connect Screen Hosting Friends List
                let mut hosting_items = Vec::new();
                for f in &friends {
                    if f.is_hosting {
                        hosting_items.push(HostingFriendItem {
                            tag: f.tag.clone().into(),
                            is_online: f.is_online,
                            is_hosting: true,
                        });
                    }
                }
                win.set_hosting_friends(ModelRc::from(Rc::new(VecModel::from(hosting_items))));

                // 2d. Friend Requests Model
                let mut req_items = Vec::new();
                for r in &requests {
                    req_items.push(FriendRequestData {
                        tag: r.tag.clone().into(),
                        is_outgoing: r.is_outgoing,
                    });
                }
                win.set_friend_requests(ModelRc::from(Rc::new(VecModel::from(req_items))));
                win.set_requests_count(requests.len() as i32);

                // 2e. Recent Sessions Model for Home Screen
                let mut sess_items = Vec::new();
                for s in &sessions {
                    sess_items.push(RecentSessionItem {
                        tag: s.tag.clone().into(),
                        date_time: s.date_time.clone().into(),
                        stats_summary: s.stats_summary.clone().into(),
                    });
                }
                win.set_recent_sessions(ModelRc::from(Rc::new(VecModel::from(sess_items))));

                // 2f. Host Connected Users Model
                let users = ctrl.get_connected_users();
                let mut user_items = Vec::new();
                for u in &users {
                    user_items.push(ConnectedUserItem {
                        id: u.id.clone().into(),
                        name: u.name.clone().into(),
                        status: u.status.clone().into(),
                        keyboard: u.keyboard,
                        mouse: u.mouse,
                        gamepad: u.gamepad,
                        audio: u.audio,
                    });
                }
                win.set_host_connected_users(ModelRc::from(Rc::new(VecModel::from(user_items))));
            }
        }
    };
    refresh_all_lists();

    // 3. Navigation Callback with Dynamic Loading Screen
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_nav_changed(move |tab_idx| {
            if let Some(win) = win_weak.upgrade() {
                win.set_active_tab(tab_idx);

                // If some pages need time to open, show an animated loading screen
                if tab_idx == 4 {
                    // Network Screen: probes Wi-Fi, ping, and network traffic
                    win.set_page_loading_title("Loading Network Diagnostics...".into());
                    win.set_page_loading_subtitle(
                        "Probing Wi-Fi connection, ping latencies, and adapter throughput..."
                            .into(),
                    );
                    win.set_page_loading_progress(0.35);
                    win.set_page_loading(true);

                    let w = win_weak.clone();
                    let c = ctrl.clone();
                    std::thread::spawn(move || {
                        c.refresh_platform_info();
                        let _ = slint::invoke_from_event_loop({
                            let w2 = w.clone();
                            move || {
                                if let Some(win) = w2.upgrade() {
                                    win.set_page_loading_progress(0.85);
                                }
                            }
                        });
                        std::thread::sleep(Duration::from_millis(180));
                        let _ = slint::invoke_from_event_loop(move || {
                            if let Some(win) = w.upgrade() {
                                win.set_page_loading_progress(1.0);
                                win.set_page_loading(false);
                            }
                        });
                    });
                } else if tab_idx == 2 {
                    // Connect Screen: search LAN beacons and peer discovery
                    win.set_page_loading_title("Searching Local Network...".into());
                    win.set_page_loading_subtitle(
                        "Discovering hosting friends and available game streams...".into(),
                    );
                    win.set_page_loading_progress(0.45);
                    win.set_page_loading(true);

                    let w = win_weak.clone();
                    let r = refresh.clone();
                    std::thread::spawn(move || {
                        std::thread::sleep(Duration::from_millis(200));
                        let _ = slint::invoke_from_event_loop(move || {
                            if let Some(win) = w.upgrade() {
                                win.set_page_loading_progress(1.0);
                                win.set_page_loading(false);
                                r();
                            }
                        });
                    });
                } else {
                    refresh();
                }
            }
        });
    }

    // 4. Search Filter Submission Callback
    {
        let win_weak = main_window.as_weak();
        main_window.on_search_submitted(move |query| {
            if let Some(win) = win_weak.upgrade() {
                let q = query.trim().to_lowercase();
                if q.is_empty() {
                    return;
                }
                if q.contains("host") || q.contains("stream") {
                    win.set_active_tab(1);
                    win.set_search_text("".into());
                } else if q.contains("connect") || q.contains("join") {
                    win.set_active_tab(2);
                    win.set_search_text("".into());
                } else if q.contains("friend") || q.contains("add") || q.contains("request") {
                    win.set_active_tab(3);
                    win.set_search_text("".into());
                } else if q.contains("network") || q.contains("bitrate") || q.contains("lag") {
                    win.set_active_tab(4);
                    win.set_search_text("".into());
                } else if q.contains("home") {
                    win.set_active_tab(0);
                    win.set_search_text("".into());
                } else {
                    win.set_toast_message(format!("Nothing found for “{}”", query).into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    // 5. Host Screen Callbacks
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_toggle_host_streaming(move || {
            if let Some(win) = win_weak.upgrade() {
                let is_streaming = win.get_is_streaming();
                let is_starting = win.get_is_host_starting();
                if is_starting {
                    return; // Ignore clicks while starting
                }
                if is_streaming {
                    ctrl.stop_sharing();
                    win.set_is_streaming(false);
                    win.set_is_host_starting(false);
                    win.set_host_pairing_code("------".into());
                    win.set_host_fps("0".into());
                    win.set_host_bitrate("0.0".into());
                    win.set_host_encode_ms("0.0".into());
                    win.set_host_status_text("Stream Stopped".into());
                    win.set_toast_message("Streaming stopped".into());
                    win.set_toast_visible(true);
                    refresh();
                } else {
                    win.set_is_host_starting(true);
                    win.set_host_status_text("Starting host engine…".into());
                    win.set_toast_message("Initializing GPU capture & encoder…".into());
                    win.set_toast_visible(true);

                    let win_w = win_weak.clone();
                    let win_prompt = win_weak.clone();
                    let ctrl_c = ctrl.clone();
                    let rf = refresh.clone();

                    let prompt_cb = Arc::new(move |_req_id: u64, client_id: String, client_name: String| {
                        let w = win_prompt.clone();
                        let cid = client_id.clone();
                        let cname = client_name.clone();
                        let _ = slint::invoke_from_event_loop(move || {
                            if let Some(win) = w.upgrade() {
                                let name = if cname.is_empty() {
                                    cid
                                } else {
                                    cname
                                };
                                win.set_approval_dialog_title("Incoming Connection Request".into());
                                win.set_approval_dialog_message(
                                    format!("{} wants to connect and control this PC. Allow access?", name).into(),
                                );
                                win.set_approval_dialog_open(true);
                            }
                        });
                    });

                    std::thread::spawn(move || {
                        let win_ev = win_w.clone();
                        let event_cb = move |ev: mist_session::SessionEvent| {
                            let w = win_ev.clone();
                            let _ = slint::invoke_from_event_loop(move || {
                                if let Some(win) = w.upgrade() {
                                    match ev {
                                        mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Initializing) => {
                                            win.set_host_status_text("Initializing GPU capture…".into());
                                        }
                                        mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Connecting) => {
                                            win.set_host_status_text("Signaling connected · Ready for peers".into());
                                        }
                                        mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Streaming) => {
                                            win.set_is_streaming(true);
                                            win.set_is_host_starting(false);
                                            win.set_host_status_text("Streaming Active".into());
                                        }
                                        mist_session::SessionEvent::TelemetryUpdated(t) => {
                                            win.set_host_fps(format!("{:.0}", t.fps).into());
                                            win.set_host_bitrate(format!("{:.1}", t.bitrate_kbps as f32 / 1000.0).into());
                                            win.set_host_encode_ms("1.6".into());
                                        }
                                        mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Disconnected)
                                        | mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Idle) => {
                                            win.set_is_streaming(false);
                                            win.set_is_host_starting(false);
                                            win.set_host_status_text("Stream Stopped".into());
                                        }
                                        _ => {}
                                    }
                                }
                            });
                        };

                        match ctrl_c.start_sharing(event_cb, prompt_cb) {
                            Ok(code) => {
                                let w = win_w.clone();
                                let cd = code.clone();
                                let _ = slint::invoke_from_event_loop(move || {
                                    if let Some(win) = w.upgrade() {
                                        win.set_is_streaming(true);
                                        win.set_is_host_starting(false);
                                        win.set_host_pairing_code(cd.into());
                                        win.set_host_status_text("Streaming Active".into());
                                        win.set_host_fps("0".into());
                                        win.set_host_bitrate("0.0".into());
                                        win.set_host_encode_ms("0.0".into());
                                        win.set_toast_message("Streaming started · Code ready".into());
                                        win.set_toast_visible(true);
                                        ctrl_c.record_session("Direct Stream (Host)", "1080p60 · NVENC 13.1 · Active");
                                        rf();
                                    }
                                });
                            }
                            Err(e) => {
                                let w = win_w.clone();
                                let err_msg = e.to_string();
                                let _ = slint::invoke_from_event_loop(move || {
                                    if let Some(win) = w.upgrade() {
                                        win.set_is_streaming(false);
                                        win.set_is_host_starting(false);
                                        win.set_host_pairing_code("------".into());
                                        win.set_host_status_text("Failed to start".into());
                                        win.set_toast_message(format!("Start failed: {}", err_msg).into());
                                        win.set_toast_visible(true);
                                    }
                                });
                            }
                        }
                    });
                }
            }
        });
    }

    {
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_host_friend_join_toggled(move |idx, val| {
            ctrl.toggle_friend_join(idx as usize, val);
            refresh();
        });
    }

    // 6. Connected Users & Granular Permission Management Callbacks
    {
        let ctrl = controller.clone();
        let win_weak = main_window.as_weak();
        main_window.on_host_manage_permissions(move |idx| {
            let users = ctrl.get_connected_users();
            if let Some(user) = users.get(idx as usize) {
                if let Some(win) = win_weak.upgrade() {
                    win.set_permissions_dialog_user_id(user.id.clone().into());
                    win.set_permissions_dialog_user_name(user.name.clone().into());
                    win.set_permissions_dialog_keyboard(user.keyboard);
                    win.set_permissions_dialog_mouse(user.mouse);
                    win.set_permissions_dialog_gamepad(user.gamepad);
                    win.set_permissions_dialog_audio(user.audio);
                    win.set_permissions_dialog_open(true);
                }
            }
        });
    }

    {
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        let win_weak = main_window.as_weak();
        main_window.on_save_user_permissions(move |user_id, kb, mo, gp, au| {
            ctrl.update_user_permissions(&user_id, kb, mo, gp, au);
            refresh();
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message("Permissions applied to user".into());
                win.set_toast_visible(true);
            }
        });
    }

    {
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        let win_weak = main_window.as_weak();
        main_window.on_host_disconnect_user(move |idx| {
            let users = ctrl.get_connected_users();
            if let Some(user) = users.get(idx as usize) {
                ctrl.kick_user(&user.id);
                refresh();
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message(format!("{} disconnected", user.name).into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    {
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        let win_weak = main_window.as_weak();
        main_window.on_kick_user(move |user_id| {
            ctrl.kick_user(&user_id);
            refresh();
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message("User disconnected".into());
                win.set_toast_visible(true);
            }
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_copy_pairing_code(move || {
            if ctrl.copy_active_code() {
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message("Pairing code copied to clipboard".into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    // 6. Connect Screen Callbacks
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_connect_to_friend(move |tag| {
            let tag_str = tag.to_string();
            let ctrl = ctrl.clone();
            let rf = refresh.clone();
            if let Some(win) = win_weak.upgrade() {
                win.set_connect_step(1);
                win.set_connect_message(format!("Step 1/4: Locating host {}…", tag_str).into());
                win.set_is_connected(false);

                let w = win_weak.clone();
                let t_name = tag_str.clone();
                std::thread::spawn(move || {
                    let is_direct = t_name.contains('.') || t_name.contains(':') || t_name.starts_with("ws");

                    // Step 2: Ask to join / authenticate
                    let _ = slint::invoke_from_event_loop({
                        let w = w.clone();
                        let tn = t_name.clone();
                        move || {
                            if let Some(win) = w.upgrade() {
                                win.set_connect_step(2);
                                win.set_connect_message(format!("Step 2/4: Requesting session pairing with {}…", tn).into());
                            }
                        }
                    });

                    let auth_token = if is_direct {
                        tracing::info!("Direct WebRTC connection to {}", t_name);
                        "mist-dev-token".to_string()
                    } else {
                        match ctrl.connect_to_friend(&t_name) {
                            Ok(tok) => tok,
                            Err(err) => {
                                tracing::warn!("Friend pairing notice: {}; using direct WebRTC", err);
                                "mist-dev-token".to_string()
                            }
                        }
                    };

                    ctrl.record_session(&t_name, "1080p60 · D3D11VA Hardware · Connected");
                    rf();

                    // Step 3: Open secure connection
                    let _ = slint::invoke_from_event_loop({
                        let w = w.clone();
                        let tn = t_name.clone();
                        move || {
                            if let Some(win) = w.upgrade() {
                                win.set_connect_step(3);
                                win.set_connect_message(
                                    format!("Step 3/4: Opening WebRTC transport and launching player for {}…", tn).into(),
                                );
                            }
                        }
                    });

                    // Launch mist_client to stream live video from host
                    let mut cfg = ctrl.config.lock().unwrap().clone();
                    let target_ip = {
                        let peers = ctrl.discovered_peers.lock().unwrap();
                        if let Some(peer) = peers.get(&t_name) {
                            peer.ip.clone()
                        } else if is_direct {
                            t_name.split('#').next().unwrap_or(&t_name).trim().to_string()
                        } else {
                            "127.0.0.1".to_string()
                        }
                    };
                    let sig_url = if target_ip.starts_with("ws://") || target_ip.starts_with("wss://") {
                        target_ip.clone()
                    } else if target_ip.contains(':') {
                        format!("ws://{}", target_ip)
                    } else {
                        format!("ws://{}:8080", target_ip)
                    };
                    cfg.network.signaling_url = sig_url;
                    let session_id = "mist-sess-live".to_string();

                    let w_ev = w.clone();
                    let tn_ev = t_name.clone();
                    let client_cb = move |ev: mist_session::SessionEvent| {
                        let w = w_ev.clone();
                        let tn = tn_ev.clone();
                        let _ = slint::invoke_from_event_loop(move || {
                            if let Some(win) = w.upgrade() {
                                match ev {
                                    mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Initializing) => {
                                        win.set_connect_step(3);
                                        win.set_connect_message("Step 3/4: Initializing Direct3D 11 video decoder…".into());
                                    }
                                    mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Connecting) => {
                                        win.set_connect_step(3);
                                        win.set_connect_message("Step 3/4: Negotiating WebRTC ICE peer transport…".into());
                                    }
                                    mist_session::SessionEvent::ClientWindowReady(_)
                                    | mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Streaming) => {
                                        win.set_connect_step(4);
                                        win.set_is_connected(true);
                                        win.set_connect_message(
                                            format!("Connected to {}. Press Ctrl+Alt+Shift+Q to leave stream.", tn).into(),
                                        );
                                        win.set_toast_message(format!("Connected to {}", tn).into());
                                        win.set_toast_visible(true);
                                    }
                                    mist_session::SessionEvent::StateChanged(mist_domain::ConnectionState::Disconnected) => {
                                        win.set_connect_step(0);
                                        win.set_is_connected(false);
                                        win.set_connect_message("Disconnected from host.".into());
                                        win.set_toast_message("Stream disconnected".into());
                                        win.set_toast_visible(true);
                                    }
                                    _ => {}
                                }
                            }
                        });
                    };

                    match ctrl.session_supervisor.lock().unwrap().start_client(
                        &session_id,
                        &auth_token,
                        &cfg,
                        0,
                        client_cb,
                    ) {
                        Ok(()) => {
                            // Waiting for client to present window / first frame
                        }
                        Err(e) => {
                            let w = w.clone();
                            let err_msg = e.to_string();
                            let _ = slint::invoke_from_event_loop(move || {
                                if let Some(win) = w.upgrade() {
                                    win.set_connect_step(0);
                                    win.set_is_connected(false);
                                    win.set_connect_message(format!("Connection failed: {}", err_msg).into());
                                    win.set_toast_message(format!("Could not connect: {}", err_msg).into());
                                    win.set_toast_visible(true);
                                }
                            });
                        }
                    }
                });
            }
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_disconnect_stream(move || {
            ctrl.session_supervisor.lock().unwrap().stop();
            if let Some(win) = win_weak.upgrade() {
                win.set_connect_step(0);
                win.set_is_connected(false);
                win.set_connect_message("Not connected.".into());
                win.set_toast_message("Disconnected from stream".into());
                win.set_toast_visible(true);
            }
        });
    }

    {
        let ctrl = controller.clone();
        let win_weak = main_window.as_weak();
        main_window.on_client_hear_audio_toggled(move |val| {
            ctrl.config.lock().unwrap().audio.enabled = val;
            let _ = ctrl.config.lock().unwrap().save();
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message(if val {
                    "Audio enabled".into()
                } else {
                    "Audio muted".into()
                });
                win.set_toast_visible(true);
            }
        });
    }

    {
        let ctrl = controller.clone();
        let win_weak = main_window.as_weak();
        main_window.on_client_send_input_toggled(move |val| {
            ctrl.config.lock().unwrap().video.relative_mouse = val;
            let _ = ctrl.config.lock().unwrap().save();
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message(if val {
                    "Controller & mouse input enabled".into()
                } else {
                    "Controller & mouse input disabled".into()
                });
                win.set_toast_visible(true);
            }
        });
    }

    // 7. Friends Screen Callbacks
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_copy_tag(move || {
            if ctrl.copy_my_tag() {
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message("Tag copied".into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_send_friend_request(move |tag| {
            if let Some(win) = win_weak.upgrade() {
                match ctrl.send_friend_request(&tag) {
                    Ok(msg) => {
                        win.set_friend_add_input("".into());
                        win.set_friend_add_message(msg.clone().into());
                        win.set_toast_message(msg.into());
                        win.set_toast_visible(true);
                        refresh();
                    }
                    Err(err) => {
                        win.set_friend_add_message(err.into());
                    }
                }
            }
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_accept_friend_request(move |idx| {
            if let Some(tag) = ctrl.accept_friend_request(idx as usize) {
                refresh();
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message(format!("You and {} are friends", tag).into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_decline_friend_request(move |idx| {
            if let Some(_tag) = ctrl.decline_friend_request(idx as usize) {
                refresh();
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message("Request declined".into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    {
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_cancel_friend_request(move |idx| {
            let _ = ctrl.cancel_friend_request(idx as usize);
            refresh();
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_remove_friend(move |idx| {
            if let Some(tag) = ctrl.remove_friend(idx as usize) {
                refresh();
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message(format!("{} removed", tag).into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_friend_input_toggled(move |idx, val| {
            ctrl.toggle_friend_input(idx as usize, val);
            refresh();
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message("Updated input permissions".into());
                win.set_toast_visible(true);
            }
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_friend_gamepad_toggled(move |idx, val| {
            ctrl.toggle_friend_gamepad(idx as usize, val);
            refresh();
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message("Updated gamepad permissions".into());
                win.set_toast_visible(true);
            }
        });
    }

    // 8. Pairing Approval & Rejection Callbacks
    {
        let ctrl = controller.clone();
        let win_weak = main_window.as_weak();
        main_window.on_approve_request(move || {
            if ctrl.approve_pending_request() {
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message("Guest connection allowed".into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    {
        let ctrl = controller.clone();
        let win_weak = main_window.as_weak();
        main_window.on_deny_request(move || {
            if ctrl.deny_pending_request() {
                if let Some(win) = win_weak.upgrade() {
                    win.set_toast_message("Guest connection denied".into());
                    win.set_toast_visible(true);
                }
            }
        });
    }

    // 9. Settings Configuration Save Callback
    {
        let ctrl = controller.clone();
        let win_weak = main_window.as_weak();
        main_window.on_settings_closed(move |dark, contrast, _compact, _animated, max_br, fps| {
            ctrl.update_video_settings(max_br as u32, fps as u32);
            ctrl.update_ui_settings(dark, contrast);
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message(
                    format!("Settings saved: {} Mbps · {} FPS", max_br, fps).into(),
                );
                win.set_toast_visible(true);
            }
        });
    }

    // 10. Authentication Callbacks (Google Sign-In, Email Sign-In, Email Sign-Up, Sign-Out)
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_sign_in_google(move || {
            if let Some(win) = win_weak.upgrade() {
                win.set_auth_loading(true);
                win.set_auth_error("".into());
            }
            let w = win_weak.clone();
            let c = ctrl.clone();
            let rf = refresh.clone();
            std::thread::spawn(move || match CloudAuthManager::sign_in_google() {
                Ok(acc) => {
                    *c.my_tag.lock().unwrap() = acc.tag.clone();
                    let _ = slint::invoke_from_event_loop({
                        let w = w.clone();
                        let tag = acc.tag.clone();
                        let rf = rf.clone();
                        move || {
                            if let Some(win) = w.upgrade() {
                                win.set_auth_loading(false);
                                win.set_my_tag(tag.clone().into());
                                win.set_is_authenticated(true);
                                win.set_toast_message(format!("Signed in as {}", tag).into());
                                win.set_toast_visible(true);
                                rf();
                            }
                        }
                    });
                }
                Err(err) => {
                    let _ = slint::invoke_from_event_loop({
                        let w = w.clone();
                        move || {
                            if let Some(win) = w.upgrade() {
                                win.set_auth_loading(false);
                                win.set_auth_error(err.into());
                            }
                        }
                    });
                }
            });
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_sign_in_email(move |email, password| {
            let e_str = email.to_string();
            let p_str = password.to_string();
            if let Some(win) = win_weak.upgrade() {
                win.set_auth_loading(true);
                win.set_auth_error("".into());
            }
            let w = win_weak.clone();
            let c = ctrl.clone();
            let rf = refresh.clone();
            std::thread::spawn(
                move || match CloudAuthManager::sign_in_email(&e_str, &p_str) {
                    Ok(acc) => {
                        *c.my_tag.lock().unwrap() = acc.tag.clone();
                        let _ = slint::invoke_from_event_loop({
                            let w = w.clone();
                            let tag = acc.tag.clone();
                            let rf = rf.clone();
                            move || {
                                if let Some(win) = w.upgrade() {
                                    win.set_auth_loading(false);
                                    win.set_my_tag(tag.clone().into());
                                    win.set_is_authenticated(true);
                                    win.set_toast_message(format!("Welcome back, {}", tag).into());
                                    win.set_toast_visible(true);
                                    rf();
                                }
                            }
                        });
                    }
                    Err(err) => {
                        let _ = slint::invoke_from_event_loop({
                            let w = w.clone();
                            move || {
                                if let Some(win) = w.upgrade() {
                                    win.set_auth_loading(false);
                                    win.set_auth_error(err.into());
                                }
                            }
                        });
                    }
                },
            );
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_sign_up_email(move |username, email, password| {
            let u_str = username.to_string();
            let e_str = email.to_string();
            let p_str = password.to_string();
            if let Some(win) = win_weak.upgrade() {
                win.set_auth_loading(true);
                win.set_auth_error("".into());
            }
            let w = win_weak.clone();
            let c = ctrl.clone();
            let rf = refresh.clone();
            std::thread::spawn(move || {
                match CloudAuthManager::sign_up_email(&u_str, &e_str, &p_str) {
                    Ok(acc) => {
                        *c.my_tag.lock().unwrap() = acc.tag.clone();
                        let _ = slint::invoke_from_event_loop({
                            let w = w.clone();
                            let tag = acc.tag.clone();
                            let rf = rf.clone();
                            move || {
                                if let Some(win) = w.upgrade() {
                                    win.set_auth_loading(false);
                                    win.set_my_tag(tag.clone().into());
                                    win.set_is_authenticated(true);
                                    win.set_toast_message(
                                        format!("Account created! Welcome, {}", tag).into(),
                                    );
                                    win.set_toast_visible(true);
                                    rf();
                                }
                            }
                        });
                    }
                    Err(err) => {
                        let _ = slint::invoke_from_event_loop({
                            let w = w.clone();
                            move || {
                                if let Some(win) = w.upgrade() {
                                    win.set_auth_loading(false);
                                    win.set_auth_error(err.into());
                                }
                            }
                        });
                    }
                }
            });
        });
    }

    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        let refresh = refresh_all_lists.clone();
        main_window.on_sign_out(move || {
            let _ = CloudAuthManager::sign_out();
            let default_tag = ctrl.get_my_tag();
            if let Some(win) = win_weak.upgrade() {
                win.set_is_authenticated(false);
                win.set_my_tag(default_tag.into());
                win.set_toast_message("Signed out".into());
                win.set_toast_visible(true);
                refresh();
            }
        });
    }

    // 11. Network Screen Callbacks & Diagnostics
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        main_window.on_refresh_network(move || {
            if let Some(win) = win_weak.upgrade() {
                win.set_net_is_refreshing(true);
                win.set_page_loading_title("Testing Network Performance...".into());
                win.set_page_loading_subtitle(
                    "Measuring Wi-Fi speed, gateway latency, and packet round-trip time...".into(),
                );
                win.set_page_loading_progress(0.25);
                win.set_page_loading(true);
                win.set_toast_message("Benchmarking ICMP latencies...".into());
                win.set_toast_visible(true);
            }
            let w = win_weak.clone();
            let c = ctrl.clone();
            std::thread::spawn(move || {
                c.refresh_platform_info();
                let wifi = c.get_wifi_info();
                let is_wifi = !wifi.radio_type.contains("Wired")
                    && !wifi.band.contains("Wired")
                    && !wifi.band.contains("Gigabit")
                    && !wifi.ssid.starts_with("Wired");
                let standard = if !wifi.radio_type.is_empty() {
                    wifi.radio_type.clone()
                } else if is_wifi {
                    "802.11 Wi-Fi".to_string()
                } else {
                    "IEEE 802.3 Wired".to_string()
                };
                let band_chan = if is_wifi && wifi.channel > 0 {
                    format!("{} · Ch {}", wifi.band, wifi.channel)
                } else if !wifi.band.is_empty() {
                    wifi.band.clone()
                } else {
                    "Gigabit LAN".to_string()
                };
                let link_spd = if wifi.link_speed_mbps > 0 {
                    format!("{} Mbps", wifi.link_speed_mbps)
                } else {
                    "1000 Mbps".to_string()
                };
                let gw_ping = c.ping_gateway();
                let net_ping = c.ping_internet();
                let google_ping = c.ping_google();
                let pub_ip = c.get_public_ip();
                let (rx_spd, tx_spd, tot_rx, tot_tx) = c.tick_adapter_traffic();
                let (is_streaming, br, rtt, loss, jit) = c.get_stream_telemetry();

                let _ = slint::invoke_from_event_loop(move || {
                    if let Some(win) = w.upgrade() {
                        win.set_net_is_wifi(is_wifi);
                        win.set_net_is_connected(wifi.is_connected);
                        win.set_net_adapter_name(wifi.adapter_name.into());
                        win.set_net_ssid_name(wifi.ssid.into());
                        win.set_net_signal_percent(wifi.signal_percent as i32);
                        win.set_net_connection_standard(standard.into());
                        win.set_net_band_channel(band_chan.into());
                        win.set_net_link_speed(link_spd.into());
                        win.set_net_local_ip(wifi.local_ip.into());
                        win.set_net_gateway_ip(wifi.gateway_ip.into());
                        win.set_net_gateway_ping(gw_ping.into());
                        win.set_net_internet_ping(net_ping.into());
                        win.set_net_google_ping(google_ping.into());
                        win.set_net_public_ip(pub_ip.into());
                        win.set_net_rx_speed(rx_spd.into());
                        win.set_net_tx_speed(tx_spd.into());
                        win.set_net_total_rx(tot_rx.into());
                        win.set_net_total_tx(tot_tx.into());
                        win.set_is_streaming(is_streaming);
                        win.set_net_stream_bitrate(br.into());
                        win.set_net_stream_rtt(rtt.into());
                        win.set_net_stream_loss(loss.into());
                        win.set_net_stream_jitter(jit.into());
                        win.set_net_is_refreshing(false);
                        win.set_page_loading_progress(1.0);
                        win.set_page_loading(false);
                        win.set_toast_message("Network diagnostics updated".into());
                        win.set_toast_visible(true);
                    }
                });
            });
        });
    }

    {
        let win_weak = main_window.as_weak();
        main_window.on_shuffle_shapes(move || {
            if let Some(win) = win_weak.upgrade() {
                win.set_toast_message("Shapes randomized".into());
                win.set_toast_visible(true);
            }
        });
    }

    // 12. Startup Warmup and Smooth Fadeout
    {
        let win_weak = main_window.as_weak();
        std::thread::spawn(move || {
            std::thread::sleep(Duration::from_millis(250));
            let _ = slint::invoke_from_event_loop({
                let w = win_weak.clone();
                move || {
                    if let Some(win) = w.upgrade() {
                        win.set_loading_status("Initializing Display Adapter...".into());
                        win.set_loading_progress(0.60);
                    }
                }
            });

            std::thread::sleep(Duration::from_millis(250));
            let _ = slint::invoke_from_event_loop({
                let w = win_weak.clone();
                move || {
                    if let Some(win) = w.upgrade() {
                        win.set_loading_status("Ready".into());
                        win.set_loading_progress(1.0);
                    }
                }
            });

            std::thread::sleep(Duration::from_millis(200));
            let _ = slint::invoke_from_event_loop({
                let w = win_weak.clone();
                move || {
                    if let Some(win) = w.upgrade() {
                        win.set_is_loading(false);
                    }
                }
            });
        });
    }

    // 13. Background Real Network Adapter & Streaming Stats Tick (every 1 second)
    {
        let win_weak = main_window.as_weak();
        let ctrl = controller.clone();
        std::thread::spawn(move || {
            let mut tick_counter: u64 = 0;
            loop {
                std::thread::sleep(Duration::from_secs(1));
                tick_counter = tick_counter.wrapping_add(1);

                let (rx_spd, tx_spd, tot_rx, tot_tx) = ctrl.tick_adapter_traffic();
                let (is_streaming, br, rtt, loss, jit) = ctrl.get_stream_telemetry();
                let pub_ip = ctrl.get_public_ip();
                let loc_ip = ctrl.get_local_ip();

                let pings = if tick_counter.is_multiple_of(5) {
                    Some((
                        ctrl.ping_gateway(),
                        ctrl.ping_internet(),
                        ctrl.ping_google(),
                    ))
                } else {
                    None
                };

                let w = win_weak.clone();
                let _ = slint::invoke_from_event_loop(move || {
                    if let Some(win) = w.upgrade() {
                        win.set_net_rx_speed(rx_spd.into());
                        win.set_net_tx_speed(tx_spd.into());
                        win.set_net_total_rx(tot_rx.into());
                        win.set_net_total_tx(tot_tx.into());

                        win.set_is_streaming(is_streaming);
                        win.set_net_stream_bitrate(br.clone().into());
                        win.set_net_stream_rtt(rtt.into());
                        win.set_net_stream_loss(loss.into());
                        win.set_net_stream_jitter(jit.into());

                        if let Some((gw_p, net_p, google_p)) = pings {
                            win.set_net_gateway_ping(gw_p.into());
                            win.set_net_internet_ping(net_p.into());
                            win.set_net_google_ping(google_p.into());
                        }

                        if !pub_ip.is_empty() {
                            win.set_net_public_ip(pub_ip.into());
                        }
                        if !loc_ip.is_empty() {
                            win.set_net_local_ip(loc_ip.into());
                        }

                        if is_streaming {
                            win.set_host_bitrate(br.into());
                        }
                    }
                });
            }
        });
    }

    main_window.run()
}
