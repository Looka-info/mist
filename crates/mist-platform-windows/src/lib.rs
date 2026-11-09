//! Native Windows platform utilities: WLAN queries, hardware checks, window embedding, and clipboard.

use std::process::Command;
use windows_sys::Win32::Foundation::{HANDLE, HWND};
use windows_sys::Win32::System::DataExchange::{
    CloseClipboard, EmptyClipboard, GetClipboardData, OpenClipboard, SetClipboardData,
};
use windows_sys::Win32::System::Memory::{GlobalAlloc, GlobalLock, GlobalUnlock, GHND};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    GetWindowLongPtrW, SetParent, SetWindowLongPtrW, SetWindowPos, GWL_STYLE, HWND_TOP,
    SWP_NOACTIVATE, SWP_NOZORDER, SWP_SHOWWINDOW, SWP_HIDEWINDOW, WS_CAPTION, WS_CHILD,
    WS_CLIPSIBLINGS, WS_POPUP, WS_THICKFRAME, WS_VISIBLE,
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
}

pub fn query_wifi_info() -> WifiInfo {
    let mut info = WifiInfo::default();

    // Query via netsh wlan show interfaces
    let output = Command::new("netsh")
        .args(["wlan", "show", "interfaces"])
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

    info
}

#[derive(Debug, Clone, Default)]
pub struct HardwareInfo {
    pub gpu_name: String,
    pub nvenc_supported: bool,
    pub audio_ready: bool,
}

pub fn query_hardware_info() -> HardwareInfo {
    let mut hw = HardwareInfo {
        gpu_name: "Direct3D 11 Display Adapter".to_string(),
        nvenc_supported: false,
        audio_ready: true,
    };

    // Query GPU via wmic or powershell
    let output = Command::new("powershell")
        .args(["-NoProfile", "-Command", "Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name"])
        .output();

    if let Ok(out) = output {
        let text = String::from_utf8_lossy(&out.stdout);
        for line in text.lines() {
            let name = line.trim();
            if !name.is_empty() {
                hw.gpu_name = name.to_string();
                if name.to_lowercase().contains("nvidia") || name.to_lowercase().contains("geforce") || name.to_lowercase().contains("rtx") || name.to_lowercase().contains("gtx") {
                    hw.nvenc_supported = true;
                    break;
                }
            }
        }
    }

    hw
}

pub unsafe fn embed_child_window(child_hwnd: u64, parent_hwnd: u64, x: i32, y: i32, width: u32, height: u32) {
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
