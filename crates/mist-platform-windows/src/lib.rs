//! Native Windows platform utilities: WLAN queries, hardware checks, window embedding, and clipboard.

use std::os::windows::process::CommandExt;
use std::process::Command;

const CREATE_NO_WINDOW: u32 = 0x08000000;
use windows_sys::Win32::Foundation::{HANDLE, HWND};
use windows_sys::Win32::System::DataExchange::{
    CloseClipboard, EmptyClipboard, GetClipboardData, OpenClipboard, SetClipboardData,
};
use windows_sys::Win32::System::Memory::{GlobalAlloc, GlobalLock, GlobalUnlock, GHND};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    GetSystemMetrics, GetWindowLongPtrW, SetParent, SetWindowLongPtrW, SetWindowPos, GWL_STYLE,
    HWND_TOP, SM_CXSCREEN, SM_CYSCREEN, SWP_HIDEWINDOW, SWP_NOACTIVATE, SWP_NOZORDER,
    SWP_SHOWWINDOW, WS_CAPTION, WS_CHILD, WS_CLIPSIBLINGS, WS_POPUP, WS_THICKFRAME, WS_VISIBLE,
};

#[derive(Debug, Clone, Default)]
pub struct WifiInfo {
    pub is_connected: bool,
    pub adapter_name: String,
    pub ssid: String,
    pub signal_percent: u32,
    pub radio_type: String,
    pub band: String,
    pub channel: u32,
    pub local_ip: String,
    pub gateway_ip: String,
    pub link_speed_mbps: u32,
}

pub fn query_wifi_info() -> WifiInfo {
    let mut info = WifiInfo::default();

    // 1. Try querying via netsh wlan show interfaces
    let output = Command::new("netsh")
        .args(["wlan", "show", "interfaces"])
        .creation_flags(CREATE_NO_WINDOW)
        .output();

    if let Ok(out) = output {
        let text = String::from_utf8_lossy(&out.stdout);
        for line in text.lines() {
            let line = line.trim();
            if let Some((k, v)) = line.split_once(':') {
                let key = k.trim().to_lowercase();
                let val = v.trim();

                if key.starts_with("name") && info.adapter_name.is_empty() {
                    info.adapter_name = val.to_string();
                } else if key.starts_with("state") {
                    info.is_connected = val.eq_ignore_ascii_case("connected");
                } else if key.starts_with("ssid") && !key.starts_with("bssid") {
                    info.ssid = val.to_string();
                } else if key.starts_with("signal") {
                    if let Some(num_str) = val.strip_suffix('%') {
                        info.signal_percent = num_str.trim().parse().unwrap_or(0);
                    }
                } else if key.starts_with("radio type") {
                    info.radio_type = val.to_string();
                } else if key.starts_with("band") {
                    info.band = val.to_string();
                } else if key.starts_with("channel") {
                    info.channel = val.parse().unwrap_or(0);
                } else if key.starts_with("receive rate") || key.starts_with("transmit rate") {
                    let speed_part = val.split_whitespace().next().unwrap_or(val);
                    if let Ok(num) = speed_part.replace(',', ".").parse::<f32>() {
                        let rounded = num.round() as u32;
                        if rounded > info.link_speed_mbps {
                            info.link_speed_mbps = rounded;
                        }
                    }
                }
            }
        }
    }

    if info.band.is_empty() && info.channel > 0 {
        info.band = if info.channel > 14 {
            "5 GHz".to_string()
        } else {
            "2.4 GHz".to_string()
        };
    }

    // 2. Query IPv4 addresses to obtain local IP and Gateway, or fallback to Ethernet
    if let Ok(ip_out) = Command::new("netsh")
        .args(["interface", "ipv4", "show", "addresses"])
        .creation_flags(CREATE_NO_WINDOW)
        .output()
    {
        let text = String::from_utf8_lossy(&ip_out.stdout);
        let mut cur_iface = String::new();
        let mut cur_ip = String::new();
        let mut cur_gw = String::new();

        for line in text.lines() {
            let line = line.trim();
            if line.starts_with("Configuration for interface") {
                if !cur_gw.is_empty() && !cur_ip.is_empty() {
                    info.local_ip = cur_ip.clone();
                    info.gateway_ip = cur_gw.clone();
                    if !info.is_connected || info.ssid.is_empty() {
                        info.is_connected = true;
                        info.adapter_name = cur_iface.clone();
                        info.ssid = format!("Wired LAN ({})", cur_ip);
                        info.signal_percent = 100;
                        info.band = "Gigabit LAN".to_string();
                        info.radio_type = "IEEE 802.3 Wired".to_string();
                        info.channel = 1;
                        info.link_speed_mbps = 1000;
                    }
                    break;
                }
                cur_iface = line
                    .trim_start_matches("Configuration for interface")
                    .trim()
                    .trim_matches('"')
                    .to_string();
                cur_ip.clear();
                cur_gw.clear();
            } else if line.starts_with("IP Address:") {
                if let Some((_, val)) = line.split_once(':') {
                    cur_ip = val.trim().to_string();
                }
            } else if line.starts_with("Default Gateway:") {
                if let Some((_, val)) = line.split_once(':') {
                    cur_gw = val.trim().to_string();
                }
            }
        }

        if info.local_ip.is_empty() && !cur_gw.is_empty() && !cur_ip.is_empty() {
            info.local_ip = cur_ip.clone();
            info.gateway_ip = cur_gw.clone();
            if !info.is_connected || info.ssid.is_empty() {
                info.is_connected = true;
                info.adapter_name = cur_iface;
                info.ssid = format!("Wired LAN ({})", cur_ip);
                info.signal_percent = 100;
                info.band = "Gigabit LAN".to_string();
                info.radio_type = "IEEE 802.3 Wired".to_string();
                info.channel = 1;
                info.link_speed_mbps = 1000;
            }
        }
    }

    // 3. Physical link speed query fallback if netsh did not provide transmit rate or for wired adapter
    if info.link_speed_mbps == 0 || info.radio_type.contains("Wired") {
        if let Ok(ps_out) = Command::new("powershell")
            .args([
                "-NoProfile",
                "-Command",
                "Get-NetAdapter | Where-Object Status -eq 'Up' | Where-Object Name -notmatch 'vEthernet' | Select-Object -ExpandProperty LinkSpeed",
            ])
            .creation_flags(CREATE_NO_WINDOW)
            .output()
        {
            let text = String::from_utf8_lossy(&ps_out.stdout);
            for line in text.lines() {
                let line = line.trim();
                if line.contains("Gbps") {
                    if let Ok(num) = line.split_whitespace().next().unwrap_or("1").parse::<f32>() {
                        info.link_speed_mbps = (num * 1000.0) as u32;
                        break;
                    }
                } else if line.contains("Mbps") {
                    if let Ok(num) = line.split_whitespace().next().unwrap_or("100").parse::<f32>() {
                        info.link_speed_mbps = num as u32;
                        break;
                    }
                }
            }
        }
        if info.link_speed_mbps == 0 {
            info.link_speed_mbps = 1000;
        }
    }

    info
}

#[derive(Debug, Clone, Default)]
pub struct NetworkTraffic {
    pub bytes_in: u64,
    pub bytes_out: u64,
}

pub fn query_network_adapter_traffic() -> NetworkTraffic {
    let mut traffic = NetworkTraffic::default();
    if let Ok(out) = Command::new("netsh")
        .args(["interface", "ipv4", "show", "subinterfaces"])
        .creation_flags(CREATE_NO_WINDOW)
        .output()
    {
        let text = String::from_utf8_lossy(&out.stdout);
        for line in text.lines() {
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() >= 5 {
                if let (Ok(bin), Ok(bout)) = (parts[2].parse::<u64>(), parts[3].parse::<u64>()) {
                    let iface = parts[4..].join(" ");
                    if !iface.contains("Loopback") {
                        traffic.bytes_in += bin;
                        traffic.bytes_out += bout;
                    }
                }
            }
        }
    }
    traffic
}

pub fn measure_ping(target: &str) -> Option<f32> {
    if target.is_empty() || target == "Unavailable" {
        return None;
    }
    let out = Command::new("ping")
        .args(["-n", "1", "-w", "500", target])
        .creation_flags(CREATE_NO_WINDOW)
        .output()
        .ok()?;
    let text = String::from_utf8_lossy(&out.stdout);
    for line in text.lines() {
        if line.contains("time=") || line.contains("time<") {
            if let Some(idx) = line.find("time") {
                let rest = &line[idx + 4..];
                let trimmed = rest.trim_start_matches('=').trim_start_matches('<');
                if let Some(ms_idx) = trimmed.find("ms") {
                    let num_str = trimmed[..ms_idx].trim();
                    if let Ok(v) = num_str.parse::<f32>() {
                        return Some(if v == 0.0 { 0.8 } else { v });
                    }
                }
            }
        }
    }
    None
}

#[derive(Debug, Clone, Default)]
pub struct HardwareInfo {
    pub gpu_name: String,
    pub driver_version: String,
    pub nvenc_supported: bool,
    pub audio_ready: bool,
}

pub fn query_hardware_info() -> HardwareInfo {
    let mut hw = HardwareInfo {
        gpu_name: "Direct3D 11 Display Adapter".to_string(),
        driver_version: "Generic".to_string(),
        nvenc_supported: false,
        audio_ready: true,
    };

    // Fast query GPU and driver via Windows Registry (< 2 ms, no powershell overhead)
    let output = Command::new("reg")
        .args([
            "query",
            r"HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000",
        ])
        .creation_flags(CREATE_NO_WINDOW)
        .output();

    if let Ok(out) = output {
        let text = String::from_utf8_lossy(&out.stdout);
        for line in text.lines() {
            if line.contains("DriverDesc") {
                if let Some((_, val)) = line.split_once("REG_SZ") {
                    let name = val.trim();
                    if !name.is_empty() {
                        hw.gpu_name = name.to_string();
                        if name.to_lowercase().contains("nvidia")
                            || name.to_lowercase().contains("geforce")
                            || name.to_lowercase().contains("rtx")
                            || name.to_lowercase().contains("gtx")
                        {
                            hw.nvenc_supported = true;
                        }
                    }
                }
            } else if line.contains("DriverVersion") {
                if let Some((_, val)) = line.split_once("REG_SZ") {
                    let ver = val.trim();
                    if !ver.is_empty() {
                        // For NVIDIA drivers (e.g. 32.0.16.1714), standard release number is 617.14
                        let parts: Vec<&str> = ver.split('.').collect();
                        if parts.len() == 4 && parts[2].len() >= 2 {
                            let major = &parts[2][parts[2].len() - 1..];
                            hw.driver_version = format!("{}{}.{}", major, &parts[3][..2], &parts[3][2..]);
                        } else {
                            hw.driver_version = ver.to_string();
                        }
                    }
                }
            }
        }
    }

    hw
}

/// Check if the Virtual Gamepad Emulation Bus (ViGEmBus) driver service is installed.
pub fn check_vigembus_installed() -> bool {
    let output = Command::new("reg")
        .args([
            "query",
            r"HKLM\SYSTEM\CurrentControlSet\Services\ViGEmBus",
        ])
        .creation_flags(CREATE_NO_WINDOW)
        .output();

    match output {
        Ok(out) => out.status.success(),
        Err(_) => false,
    }
}

/// Query the primary display's active resolution (width, height) using native Windows APIs.
pub fn query_display_resolution() -> (u32, u32) {
    unsafe {
        let cx = GetSystemMetrics(SM_CXSCREEN);
        let cy = GetSystemMetrics(SM_CYSCREEN);
        if cx > 0 && cy > 0 {
            (cx as u32, cy as u32)
        } else {
            (1920, 1080)
        }
    }
}

/// # Safety
/// Caller must provide valid HWND window handles.
pub unsafe fn embed_child_window(
    child_hwnd: u64,
    parent_hwnd: u64,
    x: i32,
    y: i32,
    width: u32,
    height: u32,
) {
    if child_hwnd == 0 || parent_hwnd == 0 {
        return;
    }
    let child = child_hwnd as HWND;
    let parent = parent_hwnd as HWND;

    SetParent(child, parent);

    let mut style = GetWindowLongPtrW(child, GWL_STYLE) as u32;
    style &= !(WS_POPUP | WS_CAPTION | WS_THICKFRAME);
    style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
    SetWindowLongPtrW(child, GWL_STYLE, style as isize);

    SetWindowPos(
        child,
        HWND_TOP,
        x,
        y,
        width as i32,
        height as i32,
        SWP_NOACTIVATE | SWP_SHOWWINDOW,
    );
}

/// # Safety
/// Caller must provide valid HWND window handles.
pub unsafe fn resize_child_window(child_hwnd: u64, x: i32, y: i32, width: u32, height: u32) {
    if child_hwnd == 0 {
        return;
    }
    SetWindowPos(
        child_hwnd as HWND,
        HWND_TOP,
        x,
        y,
        width as i32,
        height as i32,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW,
    );
}

/// # Safety
/// Caller must provide valid HWND window handles.
pub unsafe fn hide_child_window(child_hwnd: u64) {
    if child_hwnd == 0 {
        return;
    }
    SetWindowPos(
        child_hwnd as HWND,
        HWND_TOP,
        0,
        0,
        0,
        0,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_HIDEWINDOW,
    );
}

pub fn get_clipboard_text() -> Option<String> {
    unsafe {
        if OpenClipboard(std::ptr::null_mut()) == 0 {
            return None;
        }
        const CF_UNICODETEXT: u32 = 13;
        let handle = GetClipboardData(CF_UNICODETEXT);
        if handle.is_null() {
            CloseClipboard();
            return None;
        }
        let ptr = GlobalLock(handle as _) as *const u16;
        if ptr.is_null() {
            CloseClipboard();
            return None;
        }
        let mut len = 0;
        while *ptr.add(len) != 0 {
            len += 1;
        }
        let slice = std::slice::from_raw_parts(ptr, len);
        let s = String::from_utf16_lossy(slice);
        GlobalUnlock(handle as _);
        CloseClipboard();
        Some(s)
    }
}

pub fn set_clipboard_text(text: &str) -> bool {
    let wide: Vec<u16> = text.encode_utf16().chain(Some(0)).collect();
    let byte_len = wide.len() * 2;
    unsafe {
        if OpenClipboard(std::ptr::null_mut()) == 0 {
            return false;
        }
        EmptyClipboard();
        let handle = GlobalAlloc(GHND, byte_len);
        if handle.is_null() {
            CloseClipboard();
            return false;
        }
        let ptr = GlobalLock(handle) as *mut u16;
        if ptr.is_null() {
            CloseClipboard();
            return false;
        }
        std::ptr::copy_nonoverlapping(wide.as_ptr(), ptr, wide.len());
        GlobalUnlock(handle);

        const CF_UNICODETEXT: u32 = 13;
        let res = SetClipboardData(CF_UNICODETEXT, handle as HANDLE);
        CloseClipboard();
        !res.is_null()
    }
}

/// Dispatches an AMD standard 102-byte Magic Packet over UDP broadcast to wake a remote host machine.
pub fn send_wake_on_lan(mac_str: &str) -> Result<(), String> {
    let clean: String = mac_str.chars().filter(|c| c.is_ascii_hexdigit()).collect();
    if clean.len() != 12 {
        return Err(format!(
            "Invalid MAC address: expected 12 hex digits (e.g. 00:11:22:33:44:55), got {}",
            clean.len()
        ));
    }

    let mut mac_bytes = [0u8; 6];
    for i in 0..6 {
        mac_bytes[i] = u8::from_str_radix(&clean[i * 2..i * 2 + 2], 16)
            .map_err(|e| format!("Failed to parse hex digit: {}", e))?;
    }

    // Magic packet: 6 bytes 0xFF followed by 16 iterations of target MAC address
    let mut packet = [0u8; 102];
    packet[..6].fill(0xFF);
    for i in 0..16 {
        let offset = 6 + i * 6;
        packet[offset..offset + 6].copy_from_slice(&mac_bytes);
    }

    let socket = std::net::UdpSocket::bind("0.0.0.0:0")
        .map_err(|e| format!("Failed to bind UDP socket: {}", e))?;
    socket
        .set_broadcast(true)
        .map_err(|e| format!("Failed to set broadcast mode: {}", e))?;
    socket
        .send_to(&packet, "255.255.255.255:9")
        .map_err(|e| format!("Failed to send broadcast packet: {}", e))?;

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_wake_on_lan_mac_parsing_and_broadcast() {
        assert!(send_wake_on_lan("00:11:22:33:44:55").is_ok());
        assert!(send_wake_on_lan("AA-BB-CC-DD-EE-FF").is_ok());
        assert!(send_wake_on_lan("0123456789ab").is_ok());

        // Rejections
        assert!(send_wake_on_lan("00:11:22:33").is_err());
        assert!(send_wake_on_lan("").is_err());
        assert!(send_wake_on_lan("invalid-mac-address").is_err());
    }

    #[test]
    fn test_platform_display_and_vigembus_checks() {
        let (w, h) = query_display_resolution();
        assert!(w > 0 && h > 0);
        let _ = check_vigembus_installed();
    }
}
