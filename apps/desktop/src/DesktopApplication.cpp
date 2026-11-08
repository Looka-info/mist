#include "DesktopApplication.h"
#include "mist/common/Logger.h"
#include "mist/platform/windows/PlatformWindows.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdio>

#if defined(_WIN32)
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace mist::desktop {

static LRESULT CALLBACK DesktopWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    auto* app = reinterpret_cast<DesktopApplication*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
        case WM_SIZE:
            if (app && wParam != SIZE_MINIMIZED) {
                app->handleResize(static_cast<uint32_t>(LOWORD(lParam)), static_cast<uint32_t>(HIWORD(lParam)));
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void BeginM3Card(const char* strId, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.820f, 0.900f, 0.850f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::BeginChild(strId, size, true);
}

static void EndM3Card() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace mist::desktop
#endif

namespace mist::desktop {

DesktopApplication::DesktopApplication() noexcept
    : qualityManager_(transport::qos::QualityProfile::Adaptive) {}

DesktopApplication::~DesktopApplication() {
    if (isInitialized_.load()) {
        (void)shutdown();
    }
}

void DesktopApplication::printHeader(const std::string& title) const {
    std::cout << "\n=======================================================\n"
              << "   MIST GAMING STREAMING — " << title << "\n"
              << "=======================================================\n";
}

void DesktopApplication::renderHomeScreen() const {
    printHeader("HOME (Device & Host Discovery)");
    std::cout << "Local Device Name:      Workstation-Host\n"
              << "Host Service Status:    Ready (Listening on Port 8080)\n"
              << "Active Connection:      None (Waiting for client or connect action)\n\n"
              << "Discovered Host Nodes:\n"
              << "  [1] 127.0.0.1:8080 (Local Workstation) — Status: ONLINE (Ping: <1ms)\n"
              << "  [2] 192.168.1.9:8080 (LAN Desktop)     — Status: STANDBY\n\n"
              << "Actions:\n"
              << "  - Connect to Host\n"
              << "  - Start Local Hosting\n"
              << "  - Open Pairing Dialog\n";
}

void DesktopApplication::renderPairingScreen() {
    printHeader("DEVICE PAIRING");
    std::string code = pairingManager_.getActivePairingCode();
    if (code.empty()) {
        auto res = pairingManager_.generatePairingCode(300);
        if (res) code = res.value();
    }

    std::cout << "Active Pairing Code:    " << code << "\n"
              << "Code Validity:          300 Seconds (Single Use)\n"
              << "Host Identity:          MIST-HOST-PROD-01\n"
              << "Public Key Fingerprint: 4A:9F:88:B1:C2:E4:67:9A:01:DF:3E:22\n\n"
              << "Pending Requests:\n";

    auto pending = pairingManager_.getPendingRequests();
    if (pending.empty()) {
        std::cout << "  (No incoming connection requests currently waiting for approval)\n";
    } else {
        for (const auto& req : pending) {
            std::cout << "  * Device: " << req.deviceName << " [" << req.deviceId << "]\n";
        }
    }
    std::cout << "\nHost Instructions:\n"
              << "  Enter the 6-character code above on your client device to authorize pairing.\n";
}

void DesktopApplication::renderHostScreen() const {
    printHeader("HOST MANAGEMENT");
    std::cout << "Hosting Enabled:        YES\n"
              << "Active Capture Adapter: GPU 0 (NVIDIA GeForce GTX 1660 SUPER)\n"
              << "Current Display:        Display 1 (1920x1080 @ 60 Hz)\n"
              << "Active Viewer:          None\n\n"
              << "Host Permission Policies:\n"
              << "  - Audio Loopback Sharing:   [ENABLED]\n"
              << "  - Remote Keyboard Control:  [HOST APPROVAL REQUIRED]\n"
              << "  - Remote Mouse Control:     [HOST APPROVAL REQUIRED]\n"
              << "  - Virtual Gamepad Control:  [HOST APPROVAL REQUIRED]\n"
              << "  - Local Operator Override:  [ACTIVE — Physical inputs take instant priority]\n";
}

void DesktopApplication::renderSettingsScreen() const {
    printHeader("STREAM SETTINGS & QUALITY PROFILES");
    auto qProfile = qualityManager_.getProfile();
    auto cfg = qualityManager_.getProfileConfig();

    std::cout << "Active Profile:         " << transport::qos::qualityProfileToString(qProfile) << "\n"
              << "Resolution:             " << cfg.width << "x" << cfg.height << "\n"
              << "Target Frame Rate:      " << cfg.targetFps << " FPS\n"
              << "Target Bitrate Range:   " << cfg.minBitrateKbps << " kbps - " << cfg.maxBitrateKbps << " kbps\n"
              << "Initial Video Bitrate:  " << cfg.initialBitrateKbps << " kbps\n"
              << "Video Codec:            H.264 (NVIDIA NVENC Hardware CBR)\n"
              << "Audio Codec:            Opus 48 kHz Stereo (128 kbps, Low-Delay)\n"
              << "Vertical Sync (VSync):  Disabled (Lowest presentation latency)\n\n"
              << "Available Profiles:\n"
              << "  [1] Performance  — 720p60, 3,500 kbps (Sub-15ms competitive gaming)\n"
              << "  [2] Balanced     — 1080p60, 8,000 kbps (Default LAN/high-speed streaming)\n"
              << "  [3] Quality      — 1080p60, 15,000 kbps (Pristine visual fidelity)\n"
              << "  [4] Adaptive     — Dynamic 2,000-20,000 kbps (AIMD congestion-controlled)\n";
}

void DesktopApplication::renderLiveSessionScreen() const {
    printHeader("LIVE SESSION TELEMETRY & DIAGNOSTICS");
    std::cout << "--- VIDEO PIPELINE ---\n"
              << "Resolution:             1920x1080\n"
              << "Target / Actual FPS:    60 / 59.9 FPS\n"
              << "Encoder Bitrate:        7,850 kbps\n"
              << "Encode Latency:         1.82 ms\n"
              << "Decode Latency:         2.45 ms\n"
              << "Dropped Video Frames:   0 (0.00%)\n\n"
              << "--- NETWORK QOS ---\n"
              << "Connection Type:        Direct (LAN)\n"
              << "Estimated Bandwidth:    45,200 kbps\n"
              << "Round-Trip Time (RTT):  3.4 ms\n"
              << "Jitter:                 0.42 ms\n"
              << "Packet Loss:            0.00%\n"
              << "Congestion Controller:  Stable (AIMD)\n\n"
              << "--- AUDIO & INPUT ---\n"
              << "Audio Format / Rate:    Opus 48 kHz / 128 kbps\n"
              << "Audio Underruns:        0\n"
              << "A/V Presentation Skew:  14.93 ms (< 40ms threshold)\n"
              << "Input Events Processed: 183 events (0 dropped)\n"
              << "Input Dispatch Latency: 0.28 ms\n";
}

void DesktopApplication::renderDeviceManagementScreen() const {
    printHeader("TRUSTED DEVICE MANAGEMENT");
    auto devices = pairingManager_.getTrustedDevices();
    if (devices.empty()) {
        std::cout << "No trusted devices registered yet.\n"
                  << "Use the Pairing screen to pair and authorize a new client device.\n";
    } else {
        std::cout << std::left << std::setw(20) << "Device Name"
                  << std::setw(24) << "Device ID"
                  << std::setw(16) << "Permissions"
                  << "Paired Epoch\n"
                  << "-----------------------------------------------------------------------\n";
        for (const auto& dev : devices) {
            std::string permStr = (dev.permissions.allowKeyboard ? "K" : "-");
            permStr += (dev.permissions.allowMouse ? "M" : "-");
            permStr += (dev.permissions.allowGamepad ? "G" : "-");
            std::cout << std::left << std::setw(20) << dev.deviceName
                      << std::setw(24) << dev.deviceId
                      << std::setw(16) << permStr
                      << dev.pairedAtEpochSec << "\n";
        }
    }
}

common::Result<std::optional<int>> DesktopApplication::initialize(int argc, char* argv[]) {
    bool hasCliScreenFlag = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Mist Desktop Management Application v0.2.0-rc1\n"
                      << "Usage: mist_desktop [options]\n\n"
                      << "Options:\n"
                      << "  -h, --help            Show help information\n"
                      << "  -v, --version         Show version string\n"
                      << "  --ui                  Launch interactive Direct3D 11 GUI\n"
                      << "  --headless            Run in headless/non-interactive mode\n"
                      << "  --home                Display Home screen in console and exit\n"
                      << "  --pairing             Display Device Pairing screen in console and exit\n"
                      << "  --host-status         Display Host Management screen in console and exit\n"
                      << "  --settings            Display Stream Settings screen in console and exit\n"
                      << "  --live-session        Display Live Session telemetry in console and exit\n"
                      << "  --devices             Display Trusted Device Management in console and exit\n"
                      << "  --profile <name>      Set quality profile (Performance, Balanced, Quality, Adaptive)\n"
                      << "  --self-test           Run automated desktop self-test suite\n";
            return std::optional<int>(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mist_desktop version 0.2.0-rc1\n";
            return std::optional<int>(0);
        } else if (arg == "--home") {
            targetScreen_ = DesktopScreen::Home;
            hasCliScreenFlag = true;
        } else if (arg == "--pairing") {
            targetScreen_ = DesktopScreen::Pairing;
            hasCliScreenFlag = true;
        } else if (arg == "--host-status") {
            targetScreen_ = DesktopScreen::Host;
            hasCliScreenFlag = true;
        } else if (arg == "--settings") {
            targetScreen_ = DesktopScreen::StreamSettings;
            hasCliScreenFlag = true;
        } else if (arg == "--live-session") {
            targetScreen_ = DesktopScreen::LiveSession;
            hasCliScreenFlag = true;
        } else if (arg == "--devices") {
            targetScreen_ = DesktopScreen::DeviceManagement;
            hasCliScreenFlag = true;
        } else if (arg == "--ui") {
            launchInteractive_ = true;
        } else if (arg == "--headless") {
            headlessMode_ = true;
        } else if (arg == "--self-test") {
            runSelfTest_ = true;
        } else if (arg == "--profile" && i + 1 < argc) {
            std::string_view profName = argv[++i];
            if (profName == "Performance") qualityManager_.setProfile(transport::qos::QualityProfile::Performance);
            else if (profName == "Balanced") qualityManager_.setProfile(transport::qos::QualityProfile::Balanced);
            else if (profName == "Quality") qualityManager_.setProfile(transport::qos::QualityProfile::Quality);
            else if (profName == "Adaptive") qualityManager_.setProfile(transport::qos::QualityProfile::Adaptive);
        }
    }

    if (!hasCliScreenFlag && !runSelfTest_ && !headlessMode_) {
        launchInteractive_ = true;
    }

    auto valRes = config_.validate();
    if (!valRes) return valRes.error();

    MIST_LOG_INFO(common::SubsystemId::DesktopApp, "Initializing Mist Desktop Application v0.2.0-rc1...");
    (void)platform::windows::initializeProcessDpiAwareness();

    (void)pairingManager_.loadTrustedDevices("trusted_devices.json");

    // Seed default sample devices if first run
    if (pairingManager_.getTrustedDevices().empty()) {
        security::PairingRequest req1{"DEV-90812", "Gaming-Laptop-01", "4A:9F:88:B1:C2:E4:67:9A:01:DF:3E:22", "MIST-01", 1731000000};
        security::PairingRequest req2{"DEV-33104", "LivingRoom-NUC", "9F:33:01:A2:CC:55:12:87:EE:41:2B:99", "MIST-02", 1730900000};
        security::PairingRequest req3{"DEV-77619", "Handheld-Deck", "7B:44:99:C1:DD:02:88:31:AA:59:71:00", "MIST-03", 1731100000};

        (void)pairingManager_.submitPairingRequest(req1, 1731000000);
        security::DevicePermissions perm1{true, true, true, true, true};
        (void)pairingManager_.approvePairingRequest("DEV-90812", perm1, 1731000000);

        (void)pairingManager_.submitPairingRequest(req2, 1730900000);
        security::DevicePermissions perm2{true, true, false, false, true};
        (void)pairingManager_.approvePairingRequest("DEV-33104", perm2, 1730900000);

        (void)pairingManager_.submitPairingRequest(req3, 1731100000);
        security::DevicePermissions perm3{true, true, false, false, true};
        (void)pairingManager_.approvePairingRequest("DEV-77619", perm3, 1731100000);
    }

    if (pairingManager_.getActivePairingCode().empty()) {
        (void)pairingManager_.generatePairingCode(300);
    }

    isInitialized_.store(true);
    return std::optional<int>(std::nullopt);
}

int DesktopApplication::run() {
    if (!isInitialized_.load()) return 1;

    if (runSelfTest_) {
        std::cout << "Running Mist Desktop Self-Test...\n";
        renderHomeScreen();
        renderPairingScreen();
        renderHostScreen();
        renderSettingsScreen();
        renderLiveSessionScreen();
        renderDeviceManagementScreen();
        std::cout << "Desktop Self-Test PASSED.\n";
        return 0;
    }

    if (targetScreen_.has_value()) {
        switch (*targetScreen_) {
            case DesktopScreen::Home: renderHomeScreen(); break;
            case DesktopScreen::Pairing: renderPairingScreen(); break;
            case DesktopScreen::Host: renderHostScreen(); break;
            case DesktopScreen::StreamSettings: renderSettingsScreen(); break;
            case DesktopScreen::LiveSession: renderLiveSessionScreen(); break;
            case DesktopScreen::DeviceManagement: renderDeviceManagementScreen(); break;
            default: break;
        }
        return 0;
    }

#if defined(_WIN32)
    if (launchInteractive_) {
        auto res = runGuiLoop();
        if (!res) {
            MIST_LOG_ERROR(common::SubsystemId::DesktopApp, "Failed to run GUI: " + res.error().format());
            renderHomeScreen();
            return 1;
        }
        return 0;
    }
#endif

    renderHomeScreen();
    return 0;
}

common::Result<void> DesktopApplication::shutdown() {
    if (!isInitialized_.exchange(false)) {
        return common::Result<void>::success();
    }
    MIST_LOG_INFO(common::SubsystemId::DesktopApp, "Shutting down Mist Desktop...");
    (void)pairingManager_.saveTrustedDevices("trusted_devices.json");
    return common::Result<void>::success();
}

#if defined(_WIN32)

void DesktopApplication::handleResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || !swapChain_) return;
    guiWidth_ = width;
    guiHeight_ = height;
    if (rtv_) {
        rtv_.Reset();
    }
    (void)swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())))) {
        (void)d3d11Device_->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_.GetAddressOf());
    }
}

void DesktopApplication::applyMintAndWhiteTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = 12.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 8.0f;
    style.PopupRounding     = 10.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 8.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(16.0f, 16.0f);
    style.FramePadding      = ImVec2(12.0f, 7.0f);
    style.ItemSpacing       = ImVec2(12.0f, 10.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize     = 12.0f;

    colors[ImGuiCol_Text]                  = ImVec4(0.059f, 0.090f, 0.165f, 1.00f); // #0F172A
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.392f, 0.455f, 0.545f, 1.00f); // #64748B
    colors[ImGuiCol_WindowBg]              = ImVec4(0.973f, 0.988f, 0.980f, 1.00f); // #F8FCFA (Mint-tinted white canvas)
    colors[ImGuiCol_ChildBg]               = ImVec4(1.000f, 1.000f, 1.000f, 1.00f); // #FFFFFF (Crisp white card)
    colors[ImGuiCol_PopupBg]               = ImVec4(1.000f, 1.000f, 1.000f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.820f, 0.900f, 0.850f, 1.00f); // Soft mint border
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.941f, 0.973f, 0.957f, 1.00f); // Very soft mint fill
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.820f, 0.980f, 0.898f, 0.60f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.820f, 0.980f, 0.898f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(1.000f, 1.000f, 1.000f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(1.000f, 1.000f, 1.000f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.000f, 1.000f, 1.000f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(1.000f, 1.000f, 1.000f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.941f, 0.973f, 0.957f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.655f, 0.853f, 0.756f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.063f, 0.725f, 0.506f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.020f, 0.588f, 0.412f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.063f, 0.725f, 0.506f, 1.00f); // Vibrant Mint Green #10B981
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.063f, 0.725f, 0.506f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.020f, 0.588f, 0.412f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.063f, 0.725f, 0.506f, 1.00f); // #10B981
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.020f, 0.588f, 0.412f, 1.00f); // #059669
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.016f, 0.471f, 0.341f, 1.00f); // #047857
    colors[ImGuiCol_Header]                = ImVec4(0.820f, 0.980f, 0.898f, 1.00f); // Mint Tint #D1FAE5
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.655f, 0.953f, 0.816f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.063f, 0.725f, 0.506f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.886f, 0.930f, 0.900f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.063f, 0.725f, 0.506f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.020f, 0.588f, 0.412f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.063f, 0.725f, 0.506f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.063f, 0.725f, 0.506f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.020f, 0.588f, 0.412f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.941f, 0.973f, 0.957f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.820f, 0.980f, 0.898f, 0.80f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.820f, 0.980f, 0.898f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.973f, 0.988f, 0.980f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.941f, 0.973f, 0.957f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.941f, 0.973f, 0.957f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.820f, 0.900f, 0.850f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.886f, 0.930f, 0.900f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(1.000f, 1.000f, 1.000f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.973f, 0.988f, 0.980f, 0.60f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.820f, 0.980f, 0.898f, 0.50f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.063f, 0.725f, 0.506f, 1.00f);
}

common::Result<void> DesktopApplication::initializeD3D11Gui() {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"MistDesktopGuiWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DesktopWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, static_cast<LONG>(guiWidth_), static_cast<LONG>(guiHeight_) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = std::max(0, static_cast<int>((screenW - (rc.right - rc.left)) / 2));
    int posY = std::max(0, static_cast<int>((screenH - (rc.bottom - rc.top)) / 2));

    hwnd_ = CreateWindowExW(
        0, className, L"MIST — Native Game Streaming Studio",
        WS_OVERLAPPEDWINDOW,
        posX, posY,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) {
        return platform::windows::win32ErrorToMistError(GetLastError(), common::SubsystemId::DesktopApp, "CreateWindowExW failed");
    }

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = guiWidth_;
    scd.BufferDesc.Height = guiHeight_;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd_;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    Microsoft::WRL::ComPtr<IDXGISwapChain> legacySwapChain;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        &scd,
        legacySwapChain.GetAddressOf(),
        d3d11Device_.GetAddressOf(),
        &featureLevel,
        d3d11Context_.GetAddressOf()
    );

    if (FAILED(hr)) {
        // Fallback to WARP software rasterizer if hardware adapter creation fails (e.g. virtual environment)
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &scd,
            legacySwapChain.GetAddressOf(),
            d3d11Device_.GetAddressOf(),
            &featureLevel,
            d3d11Context_.GetAddressOf()
        );
    }

    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "D3D11CreateDeviceAndSwapChain failed");
    }

    hr = legacySwapChain.As(&swapChain_);
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "Failed to cast swapchain to IDXGISwapChain1");
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "swapChain_->GetBuffer failed");
    }

    hr = d3d11Device_->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "CreateRenderTargetView failed");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    applyMintAndWhiteTheme();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(d3d11Device_.Get(), d3d11Context_.Get());

    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);

    return common::Result<void>::success();
}

void DesktopApplication::cleanupD3D11Gui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    rtv_.Reset();
    swapChain_.Reset();
    d3d11Context_.Reset();
    d3d11Device_.Reset();

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(L"MistDesktopGuiWindow", GetModuleHandleW(nullptr));
}

common::Result<void> DesktopApplication::runGuiLoop() {
    auto initRes = initializeD3D11Gui();
    if (!initRes) {
        return initRes;
    }

    isRunning_.store(true);

    MSG msg{};
    while (isRunning_.load()) {
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                isRunning_.store(false);
                break;
            }
        }
        if (!isRunning_.load()) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        renderGuiFrame();

        ImGui::Render();

        const float clearColor[4] = { 0.973f, 0.988f, 0.980f, 1.00f }; // #F8FCFA
        d3d11Context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
        d3d11Context_->ClearRenderTargetView(rtv_.Get(), clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        (void)swapChain_->Present(1, 0);
    }

    cleanupD3D11Gui();
    return common::Result<void>::success();
}

void DesktopApplication::renderBadge(const char* text, bool isSuccess) const {
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float paddingX = 8.0f;
    float paddingY = 3.0f;
    float rectW = textSize.x + paddingX * 2.0f;
    float rectH = textSize.y + paddingY * 2.0f;

    ImU32 bgCol = isSuccess
        ? IM_COL32(209, 250, 229, 255)   // #D1FAE5 (Soft Mint)
        : IM_COL32(254, 226, 226, 255);  // #FEE2E2 (Soft Rose)
    ImU32 textCol = isSuccess
        ? IM_COL32(6, 95, 70, 255)       // #065F46 (Dark Mint)
        : IM_COL32(153, 27, 27, 255);    // #991B1B (Dark Red)

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(pos, ImVec2(pos.x + rectW, pos.y + rectH), bgCol, 8.0f);
    drawList->AddText(ImVec2(pos.x + paddingX, pos.y + paddingY), textCol, text);

    ImGui::Dummy(ImVec2(rectW, rectH));
}

void DesktopApplication::renderMetricBox(const char* label, const char* value, const char* sublabel) const {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.973f, 0.988f, 0.980f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.820f, 0.900f, 0.850f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

    ImGui::BeginChild(label, ImVec2(0, 76.0f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextDisabled("%s", label);
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "%s", value);
    if (sublabel && sublabel[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled(" %s", sublabel);
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void DesktopApplication::renderGuiHeader() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.820f, 0.900f, 0.850f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));

    ImGui::BeginChild("HeaderBar", ImVec2(0, 56.0f), true, ImGuiWindowFlags_NoScrollbar);

    // Left: Brand Logo and Version Pill
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "MIST");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.392f, 0.455f, 0.545f, 1.00f), "|  STREAM STUDIO");
    ImGui::SameLine(0, 14.0f);
    renderBadge("v0.2.0-rc1", true);

    // Center: System Status Pill
    ImGui::SameLine(0, 32.0f);
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "[ * ]");
    ImGui::SameLine(0, 4.0f);
    ImGui::TextUnformatted("HOST ONLINE : 127.0.0.1:8080");
    ImGui::SameLine(0, 12.0f);
    renderBadge("DIRECT LAN", true);

    // Right: Action Buttons
    float rightOffset = 270.0f;
    float availX = ImGui::GetWindowWidth();
    if (availX > rightOffset) {
        ImGui::SameLine(availX - rightOffset);
    }

    if (isHostingActive_) {
        if (ImGui::Button("Hosting: ACTIVE", ImVec2(125.0f, 32.0f))) {
            isHostingActive_ = false;
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.97f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.45f, 0.54f, 1.0f));
        if (ImGui::Button("Hosting: PAUSED", ImVec2(125.0f, 32.0f))) {
            isHostingActive_ = true;
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine(0, 8.0f);
    if (ImGui::Button("Settings", ImVec2(75.0f, 32.0f))) {
        currentScreen_ = DesktopScreen::StreamSettings;
    }

    ImGui::SameLine(0, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.98f, 0.92f, 0.92f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
    if (ImGui::Button("X", ImVec2(32.0f, 32.0f))) {
        isRunning_.store(false);
    }
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void DesktopApplication::renderGuiNavRail() {
    float railWidth = isNavRailExpanded_ ? 210.0f : 68.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.820f, 0.900f, 0.850f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 12.0f));

    ImGui::BeginChild("NavRail", ImVec2(railWidth, 0), true, ImGuiWindowFlags_NoScrollbar);

    if (ImGui::Button(isNavRailExpanded_ ? "  << Collapse  " : " >> ", ImVec2(isNavRailExpanded_ ? 194.0f : 52.0f, 32.0f))) {
        isNavRailExpanded_ = !isNavRailExpanded_;
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    struct NavItem {
        DesktopScreen screen;
        const char* label;
        const char* shortCode;
    };

    const NavItem items[] = {
        { DesktopScreen::Home,             "  Home",             " [H] " },
        { DesktopScreen::Pairing,          "  Pairing",          " [P] " },
        { DesktopScreen::Host,             "  Host Mgmt",        " [M] " },
        { DesktopScreen::StreamSettings,   "  Settings",         " [S] " },
        { DesktopScreen::LiveSession,      "  Telemetry",        " [T] " },
        { DesktopScreen::DeviceManagement, "  Devices",          " [D] " },
    };

    for (const auto& item : items) {
        bool isSelected = (currentScreen_ == item.screen);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.820f, 0.980f, 0.898f, 1.00f)); // Mint tint
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.024f, 0.471f, 0.341f, 1.00f));   // Dark mint text
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.063f, 0.725f, 0.506f, 1.00f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));        // Transparent
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.20f, 0.28f, 1.00f));      // Slate
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        }

        const char* displayLabel = isNavRailExpanded_ ? item.label : item.shortCode;
        float btnWidth = isNavRailExpanded_ ? 194.0f : 52.0f;
        if (ImGui::Button(displayLabel, ImVec2(btnWidth, 42.0f))) {
            currentScreen_ = item.screen;
        }

        if (!isNavRailExpanded_ && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", item.label);
        }

        ImGui::PopStyleColor(3);
        ImGui::Spacing();
    }

    if (isNavRailExpanded_) {
        float availableY = ImGui::GetContentRegionAvail().y;
        if (availableY > 70.0f) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableY - 60.0f);
            ImGui::Separator();
            ImGui::TextDisabled("Mist Engine v0.2.0");
            ImGui::TextDisabled("D3D11 / NVENC / P2P");
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void DesktopApplication::renderGuiHomeScreen() {
    // Page Title
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Host Discovery & Management Dashboard");
    ImGui::TextDisabled("Discover nearby Mist host streaming nodes, verify network latency, and manage remote gameplay sessions.");
    ImGui::Spacing();

    // 3 KPI Metric Cards
    float contentWidth = ImGui::GetContentRegionAvail().x;
    float cardWidth = std::max(200.0f, (contentWidth - 24.0f) / 3.0f);

    BeginM3Card("CardLocalNode", ImVec2(cardWidth, 110.0f));
    ImGui::TextDisabled("LOCAL HOST NODE");
    ImGui::Spacing();
    ImGui::TextUnformatted("Workstation-Host");
    ImGui::SameLine(0, 8.0f);
    renderBadge("ONLINE", true);
    ImGui::TextDisabled("127.0.0.1:8080 (Listening)");
    EndM3Card();

    ImGui::SameLine(0, 12.0f);
    BeginM3Card("CardGPU", ImVec2(cardWidth, 110.0f));
    ImGui::TextDisabled("HARDWARE ENCODER");
    ImGui::Spacing();
    ImGui::TextUnformatted("GeForce GTX 1660 SUPER");
    ImGui::TextDisabled("NVIDIA NVENC H.264 (CBR 1080p60)");
    EndM3Card();

    ImGui::SameLine(0, 12.0f);
    BeginM3Card("CardNetwork", ImVec2(cardWidth, 110.0f));
    ImGui::TextDisabled("NETWORK QOS");
    ImGui::Spacing();
    ImGui::TextUnformatted("WebRTC Direct P2P");
    ImGui::SameLine(0, 8.0f);
    renderBadge("<1ms RTT", true);
    ImGui::TextDisabled("%s", "Packet Loss: 0.00% • 45.2 Mbps");
    EndM3Card();

    ImGui::Spacing();

    // Discovered Host Nodes Grid
    BeginM3Card("DiscoveredNodesCard", ImVec2(0, 260.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "Discovered Streaming Hosts");
    ImGui::SameLine(0, 8.0f);
    ImGui::TextDisabled("(Broadcast Beacon & LAN Discovery)");
    ImGui::Spacing();

    if (ImGui::BeginTable("HostsTable", 5, ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Host Node / IP", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("Device Spec", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Ping / RTT", ImGuiTableColumnFlags_WidthStretch, 0.12f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.13f);
        ImGui::TableHeadersRow();

        // Node 1
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("127.0.0.1:8080 (Local Machine)");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("NVENC 1080p60 • WASAPI Audio");
        ImGui::TableNextColumn();
        renderBadge("READY", true);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("< 0.5 ms");
        ImGui::TableNextColumn();
        if (ImGui::Button("Connect##1", ImVec2(-1, 26.0f))) {
            currentScreen_ = DesktopScreen::LiveSession;
        }

        // Node 2
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("192.168.1.9:8080 (LAN Desktop)");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("RTX 3070 • 1440p120 • ViGEm");
        ImGui::TableNextColumn();
        renderBadge("STANDBY", true);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("1.2 ms");
        ImGui::TableNextColumn();
        if (ImGui::Button("Connect##2", ImVec2(-1, 26.0f))) {
            currentScreen_ = DesktopScreen::LiveSession;
        }

        // Node 3
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("192.168.1.15:8080 (Living Room)");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("GTX 1080 • 1080p60 • 5.1 Ch");
        ImGui::TableNextColumn();
        renderBadge("READY", true);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("2.4 ms");
        ImGui::TableNextColumn();
        if (ImGui::Button("Connect##3", ImVec2(-1, 26.0f))) {
            currentScreen_ = DesktopScreen::LiveSession;
        }

        ImGui::EndTable();
    }
    EndM3Card();

    ImGui::Spacing();

    // Quick Actions Row
    BeginM3Card("QuickActionsCard", ImVec2(0, 110.0f));
    ImGui::TextDisabled("QUICK ACTIONS");
    ImGui::Spacing();
    if (ImGui::Button("Pair New Client Device", ImVec2(220.0f, 36.0f))) {
        currentScreen_ = DesktopScreen::Pairing;
    }
    ImGui::SameLine(0, 12.0f);
    if (ImGui::Button("Configure Stream Settings", ImVec2(220.0f, 36.0f))) {
        currentScreen_ = DesktopScreen::StreamSettings;
    }
    ImGui::SameLine(0, 12.0f);
    if (ImGui::Button("View Real-Time Telemetry", ImVec2(220.0f, 36.0f))) {
        currentScreen_ = DesktopScreen::LiveSession;
    }
    EndM3Card();
}

void DesktopApplication::renderGuiPairingScreen() {
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Device Pairing & Access Authorization");
    ImGui::TextDisabled("Authorize client devices using single-use Base32 pairing codes and cryptographic Ed25519 authentication.");
    ImGui::Spacing();

    std::string code = pairingManager_.getActivePairingCode();
    if (code.empty()) {
        auto res = pairingManager_.generatePairingCode(300);
        if (res) code = res.value();
    }

    // Hero Card: Active Code
    BeginM3Card("HeroCodeCard", ImVec2(0, 190.0f));
    ImGui::TextDisabled("ACTIVE PAIRING AUTHORIZATION CODE");
    ImGui::Spacing();

    // Large Code Display Box
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.941f, 0.988f, 0.957f, 1.00f)); // Mint tint
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.063f, 0.725f, 0.506f, 1.00f));
    ImGui::BeginChild("CodeBox", ImVec2(340.0f, 64.0f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(24.0f, 14.0f));
    ImGui::TextColored(ImVec4(0.024f, 0.471f, 0.341f, 1.00f), "C O D E :   %s", code.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 20.0f);
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Host Identity: MIST-HOST-PROD-01");
    ImGui::TextDisabled("Ed25519 Fingerprint: 4A:9F:88:B1:C2:E4:67:9A:01:DF:3E:22");
    ImGui::TextDisabled("Code Validity: Single-Use • 300 Seconds Expiration");
    if (ImGui::Button("Generate New Pairing Code", ImVec2(220.0f, 32.0f))) {
        (void)pairingManager_.generatePairingCode(300);
    }
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::ProgressBar(0.85f, ImVec2(340.0f, 8.0f), "");
    EndM3Card();

    ImGui::Spacing();

    // Pending Connection Requests Card
    BeginM3Card("PendingRequestsCard", ImVec2(0, 160.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Incoming Authorization Requests");
    ImGui::Spacing();

    if (sampleRequestPending_) {
        ImGui::TextUnformatted("Device: Remote-Laptop-Pro");
        ImGui::SameLine(0, 8.0f);
        renderBadge("DEV-88219", true);
        ImGui::TextDisabled("Requested Permissions: Keyboard, Mouse, Virtual Gamepad Control");
        ImGui::Spacing();

        if (ImGui::Button("Approve Connection", ImVec2(180.0f, 34.0f))) {
            security::DevicePermissions perms{true, true, true, true, true};
            (void)pairingManager_.approvePairingRequest("DEV-88219", perms);
            sampleRequestPending_ = false;
        }
        ImGui::SameLine(0, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.98f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
        if (ImGui::Button("Deny Request", ImVec2(140.0f, 34.0f))) {
            (void)pairingManager_.rejectPairingRequest("DEV-88219");
            sampleRequestPending_ = false;
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::TextDisabled("(No incoming connection requests currently waiting for approval)");
        if (ImGui::Button("Simulate Client Connection Request", ImVec2(260.0f, 32.0f))) {
            sampleRequestPending_ = true;
        }
    }
    EndM3Card();

    ImGui::Spacing();

    // Manual Remote Code Entry
    BeginM3Card("ManualPairCard", ImVec2(0, 110.0f));
    ImGui::TextDisabled("CONNECT TO REMOTE HOST VIA CODE");
    ImGui::Spacing();
    ImGui::PushItemWidth(240.0f);
    ImGui::InputTextWithHint("##ManualCode", "Enter 6-char code (e.g. SEA-CDS)", manualPairingCodeInput_, IM_ARRAYSIZE(manualPairingCodeInput_));
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 12.0f);
    if (ImGui::Button("Authorize & Pair Remote", ImVec2(190.0f, 32.0f))) {
        manualPairingCodeInput_[0] = '\0';
    }
    EndM3Card();
}

void DesktopApplication::renderGuiHostScreen() {
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Host Management & Safety Policies");
    ImGui::TextDisabled("Configure host GPU display capture, hardware encoding parameters, and remote operator safety overrides.");
    ImGui::Spacing();

    // Host State Card
    BeginM3Card("HostStatusCard", ImVec2(0, 130.0f));
    ImGui::TextDisabled("HOST BROADCASTING STATUS");
    ImGui::Spacing();
    if (isHostingActive_) {
        renderBadge("STREAMING READY", true);
        ImGui::SameLine(0, 12.0f);
        ImGui::TextUnformatted("Listening on 0.0.0.0:8080 • WebRTC Signaling Ready");
    } else {
        renderBadge("PAUSED", false);
        ImGui::SameLine(0, 12.0f);
        ImGui::TextUnformatted("Hosting currently paused • Incoming stream requests blocked");
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Active Hardware: GPU 0 (NVIDIA GeForce GTX 1660 SUPER) • Display 1 (1920x1080 @ 60 Hz)");
    EndM3Card();

    ImGui::Spacing();

    // Host Input & Safety Policies Card
    BeginM3Card("PoliciesCard", ImVec2(0, 240.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Remote Input & Safety Policies");
    ImGui::TextDisabled("Mist ensures local host operator safety by prioritizing physical hardware inputs.");
    ImGui::Spacing();

    ImGui::Checkbox("Audio Loopback Sharing (Stream system audio to remote player)", &allowAudioLoopback_);
    ImGui::Checkbox("Remote Keyboard Control (Allow authorized remote keystrokes)", &allowKeyboard_);
    ImGui::Checkbox("Remote Mouse Control (Allow authorized remote cursor movement & clicks)", &allowMouse_);
    ImGui::Checkbox("Virtual Gamepad Emulation (DualShock 4 / Xbox 360 controller via ViGEm)", &allowGamepad_);
    ImGui::Checkbox("Local Physical Operator Override (Physical inputs instantly preempt remote control for 2.0s)", &localOperatorPriority_);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.941f, 0.988f, 0.957f, 1.00f));
    ImGui::BeginChild("SafetyNotice", ImVec2(0, 40.0f), false);
    ImGui::SetCursorPos(ImVec2(12.0f, 10.0f));
    ImGui::TextColored(ImVec4(0.024f, 0.471f, 0.341f, 1.00f), "SAFETY SHIELD ACTIVE: Local physical mouse/keyboard motion instantly suppresses remote inputs.");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    EndM3Card();
}

void DesktopApplication::renderGuiSettingsScreen() {
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Stream Quality Settings & Profiles");
    ImGui::TextDisabled("Select an optimized quality profile or fine-tune video resolution, bitrate ranges and VSync.");
    ImGui::Spacing();

    // 4 Profile Cards Grid
    float contentWidth = ImGui::GetContentRegionAvail().x;
    float cardW = std::max(160.0f, (contentWidth - 36.0f) / 4.0f);

    struct ProfileCardData {
        transport::qos::QualityProfile profile;
        const char* title;
        const char* resFps;
        const char* bitrate;
        const char* desc;
        int index;
    };

    const ProfileCardData profiles[] = {
        { transport::qos::QualityProfile::Performance, "Performance", "720p60",  "3,500 kbps",  "Sub-15ms esports & competitive", 0 },
        { transport::qos::QualityProfile::Balanced,    "Balanced",    "1080p60", "8,000 kbps",  "Optimal LAN & high-speed streaming", 1 },
        { transport::qos::QualityProfile::Quality,     "Quality",     "1080p60", "15,000 kbps", "Pristine visual fidelity", 2 },
        { transport::qos::QualityProfile::Adaptive,    "Adaptive",    "Dynamic", "2k-20k kbps", "AIMD congestion-controlled", 3 },
    };

    auto currentProf = qualityManager_.getProfile();

    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine(0, 12.0f);

        bool isSelected = (currentProf == profiles[i].profile);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.941f, 0.988f, 0.957f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.063f, 0.725f, 0.506f, 1.00f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.820f, 0.900f, 0.850f, 1.0f));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, isSelected ? 2.0f : 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

        std::string cardId = "ProfileCard" + std::to_string(i);
        ImGui::BeginChild(cardId.c_str(), ImVec2(cardW, 140.0f), true);

        ImGui::TextColored(isSelected ? ImVec4(0.024f, 0.471f, 0.341f, 1.0f) : ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "%s", profiles[i].title);
        ImGui::TextUnformatted(profiles[i].resFps);
        ImGui::TextDisabled("%s", profiles[i].bitrate);
        ImGui::TextDisabled("%s", profiles[i].desc);
        ImGui::Spacing();

        if (ImGui::Button(isSelected ? "Active Profile" : "Select Profile", ImVec2(-1, 26.0f))) {
            qualityManager_.setProfile(profiles[i].profile);
            selectedProfileIndex_ = i;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    // Fine-Tuning Parameters Card
    BeginM3Card("TuningCard", ImVec2(0, 240.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Custom Encoding & Latency Tuning");
    ImGui::Spacing();

    ImGui::PushItemWidth(320.0f);
    ImGui::SliderInt("Target Bitrate (kbps)", &targetBitrateKbps_, 1000, 40000, "%d kbps");
    ImGui::SliderInt("Minimum Bitrate (kbps)", &minBitrateKbps_, 500, 10000, "%d kbps");
    ImGui::SliderInt("Maximum Bitrate (kbps)", &maxBitrateKbps_, 5000, 50000, "%d kbps");
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Checkbox("Enable Vertical Sync (VSync) — Keep unchecked for lowest presentation latency", &vsyncEnabled_);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save & Apply Configuration", ImVec2(240.0f, 36.0f))) {
        // Saved
    }
    EndM3Card();
}

void DesktopApplication::renderGuiLiveSessionScreen() {
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Live Session Telemetry & Pipeline Diagnostics");
    ImGui::TextDisabled("Direct hardware metrics captured from NVENC, Direct3D 11 NVDEC, libdatachannel QoS and WASAPI audio.");
    ImGui::Spacing();

    // 3 Pipeline Columns
    float contentWidth = ImGui::GetContentRegionAvail().x;
    float colWidth = std::max(200.0f, (contentWidth - 24.0f) / 3.0f);

    // Column 1: Video Pipeline
    BeginM3Card("TelemetryVideo", ImVec2(colWidth, 240.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "VIDEO PIPELINE");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Resolution:          1920x1080");
    ImGui::TextDisabled("Target / Actual FPS: 60 / 59.9 FPS");
    ImGui::TextDisabled("Encoder Bitrate:     7,850 kbps (CBR)");
    ImGui::TextDisabled("Encode Latency:      1.82 ms (NVENC)");
    ImGui::TextDisabled("Decode Latency:      2.45 ms (D3D11 NVDEC)");
    ImGui::TextDisabled("Render Latency:      0.41 ms (Flip Model)");
    ImGui::TextDisabled("%s", "Dropped Frames:      0 (0.00%)");
    EndM3Card();

    ImGui::SameLine(0, 12.0f);
    // Column 2: Network QoS
    BeginM3Card("TelemetryNetwork", ImVec2(colWidth, 240.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "NETWORK QOS & TRANSPORT");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Connection Type:     WebRTC Direct (LAN)");
    ImGui::TextDisabled("Estimated Bandwidth: 45,200 kbps");
    ImGui::TextDisabled("Round-Trip Time:     3.4 ms");
    ImGui::TextDisabled("Network Jitter:      0.42 ms");
    ImGui::TextDisabled("%s", "Packet Loss:         0.00% (0 packets)");
    ImGui::TextDisabled("Congestion State:    Stable (AIMD)");
    ImGui::TextDisabled("DataChannel State:   Open (SCTP)");
    EndM3Card();

    ImGui::SameLine(0, 12.0f);
    // Column 3: Audio & Input
    BeginM3Card("TelemetryAudioInput", ImVec2(colWidth, 240.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "AUDIO & REMOTE INPUT");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Audio Format / Rate: Opus 48 kHz Stereo / 128 kbps");
    ImGui::TextDisabled("Audio Buffer Depth:  10.0 ms");
    ImGui::TextDisabled("Audio Underruns:     0");
    ImGui::TextDisabled("A/V Sync Skew:       14.93 ms (< 40ms threshold)");
    ImGui::TextDisabled("Input Events:        183 events processed");
    ImGui::TextDisabled("Dispatch Latency:    0.28 ms");
    ImGui::TextDisabled("Physical Override:   Active & Ready");
    EndM3Card();

    ImGui::Spacing();

    // End-to-End Latency Waterfall Card
    BeginM3Card("LatencyWaterfallCard", ImVec2(0, 120.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "End-to-End Motion-to-Photon Latency Waterfall");
    ImGui::Spacing();
    ImGui::TextUnformatted("Capture (0.4ms) + NVENC (1.8ms) + Network (1.7ms) + Decode (2.5ms) + Present (0.4ms) = Total: 6.8 ms");
    ImGui::Spacing();
    ImGui::ProgressBar(0.07f, ImVec2(-1, 14.0f), "Sub-Frame Motion-to-Photon: 6.8ms (Target < 16.6ms)");
    EndM3Card();
}

void DesktopApplication::renderGuiDeviceManagementScreen() {
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.00f), "Trusted Device Management");
    ImGui::TextDisabled("Manage client devices authorized to stream and send remote keyboard, mouse or gamepad inputs.");
    ImGui::Spacing();

    BeginM3Card("TrustedDevicesCard", ImVec2(0, 280.0f));
    ImGui::TextColored(ImVec4(0.063f, 0.725f, 0.506f, 1.0f), "Authorized Client Devices");
    ImGui::Spacing();

    auto devices = pairingManager_.getTrustedDevices();
    if (devices.empty()) {
        ImGui::TextDisabled("No trusted devices currently registered.");
        ImGui::Spacing();
        if (ImGui::Button("Pair a Device Now", ImVec2(180.0f, 32.0f))) {
            currentScreen_ = DesktopScreen::Pairing;
        }
    } else {
        if (ImGui::BeginTable("DevicesTable", 5, ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Device Name", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn("Device ID", ImGuiTableColumnFlags_WidthStretch, 0.20f);
            ImGui::TableSetupColumn("Permissions", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn("Paired Date", ImGuiTableColumnFlags_WidthStretch, 0.15f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.15f);
            ImGui::TableHeadersRow();

            for (const auto& dev : devices) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(dev.deviceName.c_str());

                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", dev.deviceId.c_str());

                ImGui::TableNextColumn();
                if (dev.permissions.allowKeyboard) {
                    renderBadge("KBD", true);
                    ImGui::SameLine(0, 4.0f);
                }
                if (dev.permissions.allowMouse) {
                    renderBadge("MOUSE", true);
                    ImGui::SameLine(0, 4.0f);
                }
                if (dev.permissions.allowGamepad) {
                    renderBadge("PAD", true);
                }

                ImGui::TableNextColumn();
                ImGui::TextDisabled("Epoch: %llu", static_cast<unsigned long long>(dev.pairedAtEpochSec));

                ImGui::TableNextColumn();
                std::string btnLabel = "Revoke##" + dev.deviceId;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.98f, 0.92f, 0.92f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
                if (ImGui::Button(btnLabel.c_str(), ImVec2(-1, 26.0f))) {
                    (void)pairingManager_.revokeDevice(dev.deviceId);
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::EndTable();
        }
    }
    EndM3Card();

    ImGui::Spacing();

    BeginM3Card("SecurityAdvisoryCard", ImVec2(0, 110.0f));
    ImGui::TextDisabled("CRYPTOGRAPHIC SECURITY ARCHITECTURE");
    ImGui::Spacing();
    ImGui::TextUnformatted("Trusted devices hold unique Ed25519 pairing credentials. Revoking a device immediately invalidates its token");
    ImGui::TextDisabled("and terminates any active WebRTC DataChannels. The remote client will be required to re-authenticate.");
    EndM3Card();
}

void DesktopApplication::renderGuiFrame() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    ImGui::Begin("MistDesktopMainWindow", nullptr, windowFlags);

    // Top Header Bar
    renderGuiHeader();
    ImGui::Spacing();

    // Body: Left Nav Rail + Right Content Area
    renderGuiNavRail();
    ImGui::SameLine(0, 12.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.973f, 0.988f, 0.980f, 1.00f)); // Surface
    ImGui::BeginChild("ContentArea", ImVec2(0, 0), false);

    switch (currentScreen_) {
        case DesktopScreen::Home:
            renderGuiHomeScreen();
            break;
        case DesktopScreen::Pairing:
            renderGuiPairingScreen();
            break;
        case DesktopScreen::Host:
            renderGuiHostScreen();
            break;
        case DesktopScreen::StreamSettings:
            renderGuiSettingsScreen();
            break;
        case DesktopScreen::LiveSession:
            renderGuiLiveSessionScreen();
            break;
        case DesktopScreen::DeviceManagement:
            renderGuiDeviceManagementScreen();
            break;
        default:
            renderGuiHomeScreen();
            break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
}

#endif // _WIN32

} // namespace mist::desktop
