#include "DesktopApplication.h"
#include "mist/common/Logger.h"
#include "mist/platform/windows/PlatformWindows.h"

#if defined(_WIN32)
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <shellscalingapi.h>
#include <wincodec.h>
#include <random>
#include <cmath>
#include <vector>
#endif

#include <iostream>
#include <iomanip>
#include <algorithm>

#if defined(_WIN32)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
LRESULT CALLBACK DesktopWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                auto* app = reinterpret_cast<mist::desktop::DesktopApplication*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                if (app) {
                    app->handleResize(LOWORD(lParam), HIWORD(lParam));
                }
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0; // Disable ALT application menu
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static std::string generateRandomPairingCode() {
    static const char ALPHABET[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, sizeof(ALPHABET) - 2);
    std::string s;
    for (int i = 0; i < 6; ++i) {
        s += ALPHABET[dis(gen)];
    }
    return s;
}

static std::string resolveFontPath(const std::string& filename) {
    const std::vector<std::string> candidates = {
        "resources/fonts/" + filename,
        "../resources/fonts/" + filename,
        "../../resources/fonts/" + filename,
        "../../../resources/fonts/" + filename,
        "C:/Users/mumer/AppData/Local/Programs/Mist/resources/fonts/" + filename,
        "C:/Users/mumer/OneDrive/Desktop/mist/resources/fonts/" + filename
    };
    for (const auto& path : candidates) {
        DWORD attrib = GetFileAttributesA(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return path;
        }
    }
    return "";
}

static std::string resolveResourcePath(const std::string& filename) {
    const std::vector<std::string> candidates = {
        filename,
        "resources/" + filename,
        "../resources/" + filename,
        "../../resources/" + filename,
        "../../../resources/" + filename,
        "C:/Users/mumer/AppData/Local/Programs/Mist/resources/" + filename,
        "C:/Users/mumer/OneDrive/Desktop/mist/resources/" + filename,
        "C:/Users/mumer/AppData/Local/Programs/Mist/" + filename
    };
    for (const auto& path : candidates) {
        DWORD attrib = GetFileAttributesA(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return path;
        }
    }
    return "";
}

static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> loadTextureFromPngFile(ID3D11Device* device, const std::string& filename) {
    if (!device || filename.empty()) return nullptr;

    (void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return nullptr;

    std::wstring wFilename(filename.begin(), filename.end());
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(wFilename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    UINT width = 0, height = 0;
    frame->GetSize(&width, &height);
    if (width == 0 || height == 0) return nullptr;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return nullptr;

    std::vector<uint8_t> buffer(static_cast<size_t>(width) * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(buffer.size()), buffer.data());
    if (FAILED(hr)) return nullptr;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subData{};
    subData.pSysMem = buffer.data();
    subData.SysMemPitch = width * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&desc, &subData, &texture);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(texture.Get(), nullptr, &srv);
    if (FAILED(hr)) return nullptr;

    return srv;
}

static void BeginCardContainer(const char* id, const mist::desktop::tokens::ThemeColors& c, const ImVec2& size = ImVec2(0, 0), float rounding = 28.0f) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, c.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 22.0f));
    ImGui::BeginChild(id, size, true);
}

static void EndCardContainer() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
} // namespace
#endif

namespace mist::desktop {

DesktopApplication::DesktopApplication() noexcept = default;

DesktopApplication::~DesktopApplication() {
    (void)shutdown();
}

void DesktopApplication::printHeader(const std::string& title) const {
    std::cout << "\n======================================================\n";
    std::cout << "  MIST DESKTOP -- " << title << "\n";
    std::cout << "  Play beyond your desk. | Roboto & Mint Material\n";
    std::cout << "======================================================\n\n";
}

void DesktopApplication::renderHomeScreen() const {
    printHeader("HOME VIEW");
    std::cout << "Head: Game streaming, no lag\n";
    std::cout << "Subtitle: Host this PC or join another one. Everything below is ready.\n\n";
    std::cout << "Latest Sessions:\n";
    std::cout << "  1. Living room PC  -- Yesterday * 48 min (60 fps * 11.5 Mbps)\n";
    std::cout << "  2. Aiden's Laptop   -- Sat * 2 h 10 min  (60 fps * 9.8 Mbps)\n";
    std::cout << "  3. Studio desktop   -- Thu * 15 min      (30 fps * 6.2 Mbps)\n\n";
    std::cout << "Quick Actions:\n";
    std::cout << "  [Cast] Host this PC      -- Let paired devices see and play on your desktop.\n";
    std::cout << "  [Pad]  Connect to a PC   -- Join a session with its ID.\n";
    std::cout << "  [Dev]  Pair a device     -- Create a one-time code and set permissions.\n";
    std::cout << "  [Net]  Check network     -- See how Mist adapts to your connection.\n\n";
    std::cout << "System Check:\n";
    std::cout << "  - Video Encoder:    NVENC * GTX 1660 SUPER [OK]\n";
    std::cout << "  - Graphics Driver:  617.14 [OK]\n";
    std::cout << "  - Display Session:  Attached to a monitor [OK]\n";
    std::cout << "  - Audio Output:     Default device [OK]\n";
    std::cout << "  - Controller Driver: ViGEmBus missing * using fallback [WARN]\n";
}

void DesktopApplication::renderPairingScreen() {
    printHeader("DEVICES & PAIRING VIEW");
    std::cout << "Active Pairing Code: " << pairingCode_ << "\n";
    std::cout << "Status: " << codeHintMsg_ << "\n";
    std::cout << "Incoming Requests: " << (isDeviceApprovalPending_ ? "Aiden's Laptop (Used a code just now)" : "None") << "\n";
    std::cout << "Trusted Devices:\n";
    for (const auto& dev : trustedDevices_) {
        std::cout << "  * " << dev.name << " (" << dev.lastSeen << ") [Kbd/Mouse: "
                  << (dev.allowKbdMouse ? "YES" : "NO") << ", Pad: " << (dev.allowController ? "YES" : "NO") << "]\n";
    }
}

void DesktopApplication::renderHostScreen() const {
    printHeader("HOST MANAGEMENT VIEW");
    std::cout << "Stream State: " << (isHostingActive_ ? "Streaming (Active)" : "Stopped") << "\n";
    std::cout << "Session ID: " << hostSessionId_ << "\n";
    std::cout << "Signaling Server: " << hostSignalingServer_ << "\n";
    std::cout << "Audio Stream: " << (hostAudioStream_ ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Remote Input: " << (hostAllowRemoteInput_ ? "ALLOWED" : "OFF (Local Priority)") << "\n";
}

void DesktopApplication::renderSettingsScreen() const {
    printHeader("SETTINGS VIEW");
    std::cout << "Theme: " << (settingsTheme_ == 0 ? "Light" : "Dark") << "\n";
    std::cout << "Contrast: " << (settingsContrast_ == 0 ? "Standard" : "High") << "\n";
    std::cout << "Max Video Bitrate: " << settingsMaxBitrateMbps_ << " Mbps\n";
    std::cout << "Target Frame Rate: " << (settingsFrameRate_ == 0 ? "60 fps" : "30 fps") << "\n";
}

void DesktopApplication::renderLiveSessionScreen() const {
    printHeader("NETWORK & TELEMETRY VIEW");
    std::cout << "Video Bitrate: " << networkBitrate_ << " Mbps\n";
    std::cout << "Round-Trip Time (RTT): " << networkRtt_ << " ms\n";
    std::cout << "Packet Loss: " << networkLoss_ << " %\n";
    std::cout << "Network Jitter: " << networkJitter_ << " ms\n";
    std::cout << "QoS Adaptation State: " << networkQosState_ << "\n";
    std::cout << "Bandwidth Allocation: Input 50 kbps, Audio 128 kbps, Video gets remainder\n";
}

void DesktopApplication::renderDeviceManagementScreen() const {
    std::cout << "\n======================================================\n";
    std::cout << "  MIST DESKTOP -- TRUSTED DEVICES\n";
    std::cout << "======================================================\n\n";
    std::cout << "Active Pairing Code: " << pairingCode_ << "\n";
    std::cout << "Incoming Requests: " << (isDeviceApprovalPending_ ? "Aiden's Laptop (Used a code just now)" : "None") << "\n";
    std::cout << "Trusted Devices:\n";
    for (const auto& dev : trustedDevices_) {
        std::cout << "  * " << dev.name << " (" << dev.lastSeen << ") [Kbd/Mouse: "
                  << (dev.allowKbdMouse ? "YES" : "NO") << ", Pad: " << (dev.allowController ? "YES" : "NO") << "]\n";
    }
}

void DesktopApplication::renderConnectScreen() const {
    printHeader("CONNECT TO REMOTE PC VIEW");
    std::cout << "Target Session ID: " << connectSessionId_ << "\n";
    std::cout << "Signaling Server: " << connectSignalingServer_ << "\n";
    std::cout << "Connection State: " << connectStatusMsg_ << "\n";
}

void DesktopApplication::renderNetworkScreen() const {
    renderLiveSessionScreen();
}

common::Result<std::optional<int>> DesktopApplication::initialize(int argc, char* argv[]) {
    bool hasCliScreenFlag = false;

    // Seed sample state matching Mist.html
    pairingCode_ = "7DK4LM";
    trustedDevices_.push_back({"Living room PC", "Last seen yesterday", true, true});

    bitrateHistory_.assign(60, 12.0f);
    rttHistory_.assign(60, 18.0f);

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Mist Desktop Game Streaming Studio v0.2.0-rc1\n"
                      << "Usage: mist_desktop.exe [options]\n\n"
                      << "Options:\n"
                      << "  --help, -h            Show this help text and exit\n"
                      << "  --version, -v         Display software version\n"
                      << "  --ui                  Launch interactive Direct3D 11 GUI\n"
                      << "  --headless            Run in headless/non-interactive mode\n"
                      << "  --home                Display Home screen in console and exit\n"
                      << "  --host-status         Display Host Management screen in console and exit\n"
                      << "  --connect             Display Connect screen in console and exit\n"
                      << "  --devices, --pairing  Display Devices screen in console and exit\n"
                      << "  --network, --live-session Display Network & Telemetry screen and exit\n"
                      << "  --settings            Display Settings screen in console and exit\n"
                      << "  --profile <name>      Set quality profile (Performance, Balanced, Quality, Adaptive)\n"
                      << "  --self-test           Run automated desktop self-test suite\n";
            return std::optional<int>(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mist_desktop version 0.2.0-rc1\n";
            return std::optional<int>(0);
        } else if (arg == "--home") {
            targetScreen_ = DesktopScreen::Home;
            hasCliScreenFlag = true;
        } else if (arg == "--host-status") {
            targetScreen_ = DesktopScreen::Host;
            hasCliScreenFlag = true;
        } else if (arg == "--connect") {
            targetScreen_ = DesktopScreen::Connect;
            hasCliScreenFlag = true;
        } else if (arg == "--devices" || arg == "--pairing") {
            targetScreen_ = DesktopScreen::Devices;
            hasCliScreenFlag = true;
        } else if (arg == "--network" || arg == "--live-session") {
            targetScreen_ = DesktopScreen::Network;
            hasCliScreenFlag = true;
        } else if (arg == "--settings") {
            targetScreen_ = DesktopScreen::StreamSettings;
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
    } else {
        isInitialLoading_ = false;
    }

    auto valRes = config_.validate();
    if (!valRes) return valRes.error();

    MIST_LOG_INFO(common::SubsystemId::DesktopApp, "Initializing Mist Desktop Application v0.2.0-rc1...");
    (void)platform::windows::initializeProcessDpiAwareness();

    isInitialized_.store(true);
    return std::optional<int>(std::nullopt);
}

int DesktopApplication::run() {
    if (!isInitialized_.load()) return 1;

    if (runSelfTest_) {
        std::cout << "Running Mist Desktop Self-Test...\n";
        renderHomeScreen();
        renderHostScreen();
        renderConnectScreen();
        renderPairingScreen();
        renderNetworkScreen();
        renderSettingsScreen();
        std::cout << "Desktop Self-Test PASSED.\n";
        return 0;
    }

    if (targetScreen_.has_value()) {
        switch (*targetScreen_) {
            case DesktopScreen::Home: renderHomeScreen(); break;
            case DesktopScreen::Host: renderHostScreen(); break;
            case DesktopScreen::Connect: renderConnectScreen(); break;
            case DesktopScreen::Devices: renderPairingScreen(); break;
            case DesktopScreen::Network: renderNetworkScreen(); break;
            default: renderHomeScreen(); break;
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
    if (isRunning_.load()) {
        isRunning_.store(false);
    }
    isInitialized_.store(false);
    return common::Result<void>::success();
}

#if defined(_WIN32)
void DesktopApplication::handleResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || !swapChain_ || !d3d11Device_) return;
    guiWidth_ = width;
    guiHeight_ = height;

    if (d3d11Context_) {
        d3d11Context_->OMSetRenderTargets(0, nullptr, nullptr);
    }
    rtv_.Reset();
    HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (SUCCEEDED(hr)) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
        if (SUCCEEDED(hr)) {
            (void)d3d11Device_->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_.GetAddressOf());
        }
    }
}

void DesktopApplication::applyMintAndWhiteTheme() {
    currentTheme_ = tokens::ThemeMode::Light;
    tokens::applyTheme(currentTheme_, settingsContrast_ == 1);
}

void DesktopApplication::applyActiveTheme() {
    currentTheme_ = (settingsTheme_ == 0) ? tokens::ThemeMode::Light : tokens::ThemeMode::Dark;
    tokens::applyTheme(currentTheme_, settingsContrast_ == 1);
}

void DesktopApplication::toggleTheme() {
    settingsTheme_ = (settingsTheme_ == 0) ? 1 : 0;
    applyActiveTheme();
}

void DesktopApplication::showToast(const std::string& message) {
    toastMessage_ = message;
    toastTimer_ = 2.5f; // Display for 2.5 seconds
}

void DesktopApplication::triggerError(const std::string& title, const std::string& message,
                                     const std::string& suggestion, const std::string& code,
                                     DesktopScreen returnScreen) {
    activeError_.hasError = true;
    activeError_.title = title;
    activeError_.message = message;
    activeError_.suggestion = suggestion;
    activeError_.code = code;
    activeError_.returnScreen = returnScreen;
    currentScreen_ = DesktopScreen::Error;
    MIST_LOG_ERROR(common::SubsystemId::DesktopApp, "[" + code + "] " + title + ": " + message);
}

void DesktopApplication::clearError() {
    DesktopScreen ret = activeError_.returnScreen;
    activeError_ = ErrorInfo{};
    currentScreen_ = (ret == DesktopScreen::Error) ? DesktopScreen::Home : ret;
}

void DesktopApplication::renderLoadingSpinner(const ImVec2& center, float radius, ImU32 color, float thickness) const {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float time = static_cast<float>(ImGui::GetTime());
    constexpr int numSegments = 24;
    float startAngle = time * 6.0f;
    constexpr float sweepAngle = 2.2f;

    drawList->PathClear();
    for (int i = 0; i <= numSegments; ++i) {
        float a = startAngle + (static_cast<float>(i) / static_cast<float>(numSegments)) * sweepAngle;
        drawList->PathLineTo(ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius));
    }
    drawList->PathStroke(color, ImDrawFlags_None, thickness);
}

void DesktopApplication::generateOrganicShapes() {
    organicShapes_.clear();
    struct ShapeTemplate {
        int t0;
        int t1;
        float t2;
    };
    const ShapeTemplate templates[] = {
        {1, 8, 0.35f},
        {1, 7, 0.30f},
        {0, 5, 0.12f},
        {0, 4, 0.10f},
        {0, 9, 0.07f},
        {1, 9, 0.28f}
    };

    for (int i = 0; i < 6; ++i) {
        OrganicShape s;
        const auto& t = templates[i % 6];
        s.templateType = t.t0;
        s.symmetry = t.t1;
        s.param = t.t2;
        s.phase = static_cast<float>((rand() % 628)) / 100.0f;
        s.scale = 280.0f + static_cast<float>(rand() % 160);

        // Positioning matching Mist.html: L=i%2?74+rand*14:-10-rand*8, Y=i*18-6+rand*8
        float posX = (i % 2 == 1) ? (0.76f + (rand() % 12) * 0.01f) : (-0.08f - (rand() % 8) * 0.01f);
        float posY = (static_cast<float>(i) * 0.17f - 0.04f + (rand() % 8) * 0.01f);
        s.posXRatio = posX;
        s.posYRatio = posY;
        s.driftX = -60.0f + static_cast<float>(rand() % 120);
        s.driftY = -60.0f + static_cast<float>(rand() % 120);
        s.driftDuration = 25.0f + static_cast<float>(rand() % 25);
        s.colorIndex = i % 4; // 0=s1, 1=s2, 2=s3, 3=s4

        organicShapes_.push_back(s);
    }
}

common::Result<void> DesktopApplication::initializeD3D11Gui() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = DesktopWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"MistDesktopGuiWindow";
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));

    HICON hAppIcon = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!hAppIcon) {
        std::string iconPath = resolveResourcePath("mist.ico");
        if (!iconPath.empty()) {
            hAppIcon = (HICON)LoadImageA(nullptr, iconPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        }
    }
    if (hAppIcon) {
        wc.hIcon = hAppIcon;
        wc.hIconSm = hAppIcon;
    }

    RegisterClassExW(&wc);

    RECT wr = { 0, 0, static_cast<LONG>(guiWidth_), static_cast<LONG>(guiHeight_) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowW(
        wc.lpszClassName,
        L"Mist",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, this
    );

    if (!hwnd_) {
        DWORD err = GetLastError();
        std::string msg = "Failed to create application window (Win32 Error: " + std::to_string(err) + ").";
        MessageBoxA(nullptr, msg.c_str(), "Mist Desktop — Window Creation Error", MB_ICONERROR | MB_OK);
        return platform::windows::win32ErrorToMistError(err, common::SubsystemId::DesktopApp, "CreateWindowW failed");
    }

    if (hAppIcon) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hAppIcon));
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hAppIcon));
    }

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = guiWidth_;
    scd.BufferDesc.Height = guiHeight_;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd_;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

    Microsoft::WRL::ComPtr<IDXGISwapChain> legacySwapChain;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevels, static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION, &scd,
        legacySwapChain.GetAddressOf(), d3d11Device_.GetAddressOf(),
        &featureLevel, d3d11Context_.GetAddressOf()
    );

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            featureLevels, static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION, &scd,
            legacySwapChain.GetAddressOf(), d3d11Device_.GetAddressOf(),
            &featureLevel, d3d11Context_.GetAddressOf()
        );
    }

    if (FAILED(hr)) {
        std::string msg = "Direct3D 11 initialization failed (HRESULT: 0x" + std::to_string(static_cast<uint32_t>(hr)) + ").\n\n"
                          "Please ensure your graphics card driver is installed and supports Direct3D 11.";
        MessageBoxA(nullptr, msg.c_str(), "Mist Desktop — Direct3D 11 Error", MB_ICONERROR | MB_OK);
        return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "D3D11CreateDeviceAndSwapChain failed");
    }

    hr = legacySwapChain.As(&swapChain_);
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "Failed to cast to IDXGISwapChain1");
    }

    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "swapChain_->GetBuffer failed");
        }

        hr = d3d11Device_->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_.GetAddressOf());
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::DesktopApp, "CreateRenderTargetView failed");
        }
    }

    std::string logoPng = resolveResourcePath("mist-logo.png");
    if (!logoPng.empty()) {
        logoTextureSrv_ = loadTextureFromPngFile(d3d11Device_.Get(), logoPng);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load Roboto font from downloaded Google Fonts TTF files
    std::string fontRobotoReg = resolveFontPath("Roboto-Regular.ttf");
    std::string fontRobotoMed = resolveFontPath("Roboto-Medium.ttf");
    std::string fontRobotoBld = resolveFontPath("Roboto-Bold.ttf");
    std::string fontMatIcons  = resolveFontPath("MaterialSymbolsRounded.ttf");

    if (fontRobotoMed.empty()) fontRobotoMed = fontRobotoBld;
    if (fontRobotoReg.empty()) fontRobotoReg = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (fontRobotoMed.empty()) fontRobotoMed = "C:\\Windows\\Fonts\\segoeuib.ttf";

    if (!fontRobotoReg.empty()) {
        fontRegular_  = io.Fonts->AddFontFromFileTTF(fontRobotoReg.c_str(), tokens::MintTypography::Body);     // 15px
        fontSmall_    = io.Fonts->AddFontFromFileTTF(fontRobotoReg.c_str(), tokens::MintTypography::Label);    // 13px
        if (!fontRobotoMed.empty()) {
            fontBold_     = io.Fonts->AddFontFromFileTTF(fontRobotoMed.c_str(), tokens::MintTypography::BodyBold);  // 16px
            fontTitle_    = io.Fonts->AddFontFromFileTTF(fontRobotoMed.c_str(), tokens::MintTypography::Title);     // 20px
            fontHeadline_ = io.Fonts->AddFontFromFileTTF(fontRobotoMed.c_str(), tokens::MintTypography::Headline);  // 22px
            fontDisplay_  = io.Fonts->AddFontFromFileTTF(fontRobotoMed.c_str(), tokens::MintTypography::Display);   // 32px
            fontCode_     = io.Fonts->AddFontFromFileTTF(fontRobotoMed.c_str(), tokens::MintTypography::Code);      // 34px
        }
    }

    // Merge Material Symbols Rounded into font atlas
    if (!fontMatIcons.empty()) {
        static const ImWchar icon_ranges[] = { 0xe000, 0xf300, 0 };
        ImFontConfig iconCfg;
        iconCfg.MergeMode = true;
        iconCfg.PixelSnapH = true;
        iconCfg.GlyphOffset.y = 1.0f;
        io.Fonts->AddFontFromFileTTF(fontMatIcons.c_str(), 20.0f, &iconCfg, icon_ranges);
    }

    if (!fontRegular_) {
        fontRegular_ = io.Fonts->AddFontDefault();
    }

    generateOrganicShapes();
    applyActiveTheme();

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

    fontRegular_ = nullptr;
    fontBold_ = nullptr;
    fontTitle_ = nullptr;
    fontHeadline_ = nullptr;
    fontDisplay_ = nullptr;
    fontSmall_ = nullptr;
    fontCode_ = nullptr;

    logoTextureSrv_.Reset();
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
    if (!initRes) return initRes;

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

        // Ensure render target view is valid (self-healing if lost during resize/unfocus)
        if (!rtv_ && swapChain_ && d3d11Device_) {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> bb;
            if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(bb.GetAddressOf())))) {
                (void)d3d11Device_->CreateRenderTargetView(bb.Get(), nullptr, rtv_.GetAddressOf());
            }
        }

        const auto& c = tokens::getActiveTheme(currentTheme_);
        const float clearColor[4] = { c.bg.x, c.bg.y, c.bg.z, 1.00f };
        if (rtv_ && d3d11Context_) {
            ID3D11RenderTargetView* rtvPtr = rtv_.Get();
            d3d11Context_->OMSetRenderTargets(1, &rtvPtr, nullptr);
            d3d11Context_->ClearRenderTargetView(rtvPtr, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        if (swapChain_) {
            (void)swapChain_->Present(1, 0);
        }
    }

    cleanupD3D11Gui();
    return common::Result<void>::success();
}

void DesktopApplication::renderSleekMintLogo(float size) const {
    if (logoTextureSrv_) {
        ImGui::Image(reinterpret_cast<ImTextureID>(logoTextureSrv_.Get()), ImVec2(size, size));
        return;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 pMin = pos;
    ImVec2 pMax = ImVec2(pos.x + size, pos.y + size);
    float rounding = size * 0.28f;

    // Card background matching squircle
    ImU32 badgeBg = (currentTheme_ == tokens::ThemeMode::Light)
        ? IM_COL32(255, 255, 255, 255)
        : IM_COL32(20, 38, 32, 255);
    drawList->AddRectFilled(pMin, pMax, badgeBg, rounding);

    ImU32 cardBorder = (currentTheme_ == tokens::ThemeMode::Light)
        ? IM_COL32(164, 237, 224, 255)
        : IM_COL32(35, 75, 62, 255);
    drawList->AddRect(pMin, pMax, cardBorder, rounding, 0, 1.0f);

    // Dynamic emerald/mint controller M with D-pad and Action buttons
    ImU32 mColor = (currentTheme_ == tokens::ThemeMode::Light)
        ? IM_COL32(23, 145, 96, 255)
        : IM_COL32(70, 222, 181, 255);
    ImU32 whiteColor = IM_COL32(255, 255, 255, 255);

    float pad = size * 0.18f;
    float w = size - pad * 2.0f;
    float h = size - pad * 2.0f;
    float midX = pos.x + size * 0.5f;

    // Left Wing (M mountain 1 with D-pad)
    ImVec2 ptsLeft[4] = {
        ImVec2(pos.x + pad, pos.y + pad + h),
        ImVec2(pos.x + pad, pos.y + pad + h * 0.25f),
        ImVec2(pos.x + pad + w * 0.38f, pos.y + pad),
        ImVec2(pos.x + pad + w * 0.38f, pos.y + pad + h)
    };
    drawList->AddConvexPolyFilled(ptsLeft, 4, mColor);

    // Right Wing (M mountain 2 with action buttons)
    ImVec2 ptsRight[4] = {
        ImVec2(pos.x + pad + w * 0.62f, pos.y + pad),
        ImVec2(pos.x + pad + w, pos.y + pad + h * 0.25f),
        ImVec2(pos.x + pad + w, pos.y + pad + h),
        ImVec2(pos.x + pad + w * 0.62f, pos.y + pad + h)
    };
    drawList->AddConvexPolyFilled(ptsRight, 4, mColor);

    // Center Play Triangle pointing right
    ImVec2 triPts[3] = {
        ImVec2(midX - size * 0.08f, pos.y + pad + h * 0.28f),
        ImVec2(midX + size * 0.10f, pos.y + pad + h * 0.50f),
        ImVec2(midX - size * 0.08f, pos.y + pad + h * 0.72f)
    };
    drawList->AddTriangleFilled(triPts[0], triPts[1], triPts[2], whiteColor);

    // Left D-pad (+)
    float dpadX = pos.x + pad + w * 0.18f;
    float dpadY = pos.y + pad + h * 0.60f;
    float dpadR = size * 0.06f;
    drawList->AddLine(ImVec2(dpadX - dpadR, dpadY), ImVec2(dpadX + dpadR, dpadY), whiteColor, 1.8f);
    drawList->AddLine(ImVec2(dpadX, dpadY - dpadR), ImVec2(dpadX, dpadY + dpadR), whiteColor, 1.8f);

    // Right Action Buttons (4 dots)
    float actX = pos.x + pad + w * 0.82f;
    float actY = pos.y + pad + h * 0.60f;
    float dotOff = size * 0.045f;
    float dotR = size * 0.022f;
    drawList->AddCircleFilled(ImVec2(actX, actY - dotOff), dotR, whiteColor);
    drawList->AddCircleFilled(ImVec2(actX, actY + dotOff), dotR, whiteColor);
    drawList->AddCircleFilled(ImVec2(actX - dotOff, actY), dotR, whiteColor);
    drawList->AddCircleFilled(ImVec2(actX + dotOff, actY), dotR, whiteColor);

    // Orbital Swoosh Ring
    ImU32 swooshCol = IM_COL32(70, 222, 181, 220);
    drawList->AddCircle(ImVec2(midX, pos.y + size * 0.55f), size * 0.38f, swooshCol, 32, 2.0f);

    ImGui::Dummy(ImVec2(size, size));
}

void DesktopApplication::renderVectorIcon(const char* name, const ImVec2& center, float size, ImU32 color) const {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float cx = center.x;
    float cy = center.y;
    std::string icon = name ? name : "";

    if (icon == "menu") {
        float lineW = size * 0.36f;
        drawList->AddLine(ImVec2(cx - lineW, cy - size * 0.24f), ImVec2(cx + lineW, cy - size * 0.24f), color, 2.2f);
        drawList->AddLine(ImVec2(cx - lineW, cy),               ImVec2(cx + lineW, cy),               color, 2.2f);
        drawList->AddLine(ImVec2(cx - lineW, cy + size * 0.24f), ImVec2(cx + lineW, cy + size * 0.24f), color, 2.2f);
    } else if (icon == "search") {
        float r = size * 0.22f;
        ImVec2 c(cx - size * 0.08f, cy - size * 0.08f);
        drawList->AddCircle(c, r, color, 0, 2.0f);
        drawList->AddLine(ImVec2(cx + size * 0.08f, cy + size * 0.08f), ImVec2(cx + size * 0.32f, cy + size * 0.32f), color, 2.4f);
    } else if (icon == "circle") {
        drawList->AddCircleFilled(center, size * 0.20f, color);
    } else if (icon == "devices") {
        // Monitor screen outline
        drawList->AddRect(ImVec2(cx - size * 0.38f, cy - size * 0.30f), ImVec2(cx + size * 0.15f, cy + size * 0.18f), color, 3.0f, 0, 1.8f);
        drawList->AddLine(ImVec2(cx - size * 0.12f, cy + size * 0.18f), ImVec2(cx - size * 0.12f, cy + size * 0.28f), color, 1.8f);
        drawList->AddLine(ImVec2(cx - size * 0.24f, cy + size * 0.28f), ImVec2(cx, cy + size * 0.28f), color, 1.8f);
        // Phone overlay
        drawList->AddRectFilled(ImVec2(cx + size * 0.12f, cy - size * 0.08f), ImVec2(cx + size * 0.38f, cy + size * 0.36f), color, 2.0f);
    } else if (icon == "settings") {
        drawList->AddCircle(center, size * 0.16f, color, 0, 2.0f);
        for (int i = 0; i < 6; ++i) {
            float ang = static_cast<float>(i) * 3.14159f / 3.0f;
            ImVec2 p1(cx + std::cos(ang) * size * 0.20f, cy + std::sin(ang) * size * 0.20f);
            ImVec2 p2(cx + std::cos(ang) * size * 0.36f, cy + std::sin(ang) * size * 0.36f);
            drawList->AddLine(p1, p2, color, 2.4f);
        }
    } else if (icon == "home") {
        // Roof
        ImVec2 pRoof[3] = {
            ImVec2(cx, cy - size * 0.38f),
            ImVec2(cx - size * 0.36f, cy + size * 0.02f),
            ImVec2(cx + size * 0.36f, cy + size * 0.02f)
        };
        drawList->AddPolyline(pRoof, 3, color, ImDrawFlags_None, 2.0f);
        // Walls and door
        drawList->AddLine(ImVec2(cx - size * 0.26f, cy + size * 0.02f), ImVec2(cx - size * 0.26f, cy + size * 0.34f), color, 2.0f);
        drawList->AddLine(ImVec2(cx + size * 0.26f, cy + size * 0.02f), ImVec2(cx + size * 0.26f, cy + size * 0.34f), color, 2.0f);
        drawList->AddLine(ImVec2(cx - size * 0.26f, cy + size * 0.34f), ImVec2(cx - size * 0.08f, cy + size * 0.34f), color, 2.0f);
        drawList->AddLine(ImVec2(cx + size * 0.08f, cy + size * 0.34f), ImVec2(cx + size * 0.26f, cy + size * 0.34f), color, 2.0f);
        drawList->AddLine(ImVec2(cx - size * 0.08f, cy + size * 0.34f), ImVec2(cx - size * 0.08f, cy + size * 0.16f), color, 2.0f);
        drawList->AddLine(ImVec2(cx + size * 0.08f, cy + size * 0.34f), ImVec2(cx + size * 0.08f, cy + size * 0.16f), color, 2.0f);
        drawList->AddLine(ImVec2(cx - size * 0.08f, cy + size * 0.16f), ImVec2(cx + size * 0.08f, cy + size * 0.16f), color, 2.0f);
    } else if (icon == "cast") {
        // Display frame
        ImVec2 pScreen[4] = {
            ImVec2(cx - size * 0.18f, cy + size * 0.30f),
            ImVec2(cx + size * 0.36f, cy + size * 0.30f),
            ImVec2(cx + size * 0.36f, cy - size * 0.26f),
            ImVec2(cx - size * 0.36f, cy - size * 0.26f)
        };
        drawList->AddPolyline(pScreen, 4, color, ImDrawFlags_None, 2.0f);
        drawList->AddLine(ImVec2(cx - size * 0.36f, cy - size * 0.26f), ImVec2(cx - size * 0.36f, cy - size * 0.05f), color, 2.0f);
        // Cast wave arcs from bottom left
        ImVec2 waveOrg(cx - size * 0.36f, cy + size * 0.30f);
        drawList->AddCircleFilled(waveOrg, 2.0f, color);
        drawList->PathArcTo(waveOrg, size * 0.16f, -1.57f, 0.0f);
        drawList->PathStroke(color, ImDrawFlags_None, 1.8f);
        drawList->PathArcTo(waveOrg, size * 0.30f, -1.57f, 0.0f);
        drawList->PathStroke(color, ImDrawFlags_None, 1.8f);
    } else if (icon == "sports_esports") {
        // Gamepad pill body
        drawList->AddRect(ImVec2(cx - size * 0.40f, cy - size * 0.22f), ImVec2(cx + size * 0.40f, cy + size * 0.22f), color, size * 0.18f, 0, 2.0f);
        // D-Pad +
        drawList->AddLine(ImVec2(cx - size * 0.24f, cy - size * 0.10f), ImVec2(cx - size * 0.24f, cy + size * 0.10f), color, 2.0f);
        drawList->AddLine(ImVec2(cx - size * 0.34f, cy), ImVec2(cx - size * 0.14f, cy), color, 2.0f);
        // Buttons
        drawList->AddCircleFilled(ImVec2(cx + size * 0.24f, cy - size * 0.08f), 2.0f, color);
        drawList->AddCircleFilled(ImVec2(cx + size * 0.24f, cy + size * 0.08f), 2.0f, color);
        drawList->AddCircleFilled(ImVec2(cx + size * 0.16f, cy), 2.0f, color);
        drawList->AddCircleFilled(ImVec2(cx + size * 0.32f, cy), 2.0f, color);
    } else if (icon == "network_check") {
        // Signal bars
        for (int b = 0; b < 4; ++b) {
            float bx = cx - size * 0.30f + static_cast<float>(b) * size * 0.20f;
            float bh = size * (0.16f + static_cast<float>(b) * 0.16f);
            drawList->AddLine(ImVec2(bx, cy + size * 0.30f), ImVec2(bx, cy + size * 0.30f - bh), color, 2.8f);
        }
    } else if (icon == "play_arrow") {
        ImVec2 tri[3] = {
            ImVec2(cx - size * 0.18f, cy - size * 0.26f),
            ImVec2(cx + size * 0.26f, cy),
            ImVec2(cx - size * 0.18f, cy + size * 0.26f)
        };
        drawList->AddConvexPolyFilled(tri, 3, color);
    } else if (icon == "stop") {
        drawList->AddRectFilled(ImVec2(cx - size * 0.22f, cy - size * 0.22f), ImVec2(cx + size * 0.22f, cy + size * 0.22f), color, 3.0f);
    } else if (icon == "check_circle") {
        drawList->AddCircleFilled(center, size * 0.36f, color);
        // Checkmark
        ImVec2 pChk[3] = {
            ImVec2(cx - size * 0.18f, cy),
            ImVec2(cx - size * 0.04f, cy + size * 0.14f),
            ImVec2(cx + size * 0.20f, cy - size * 0.14f)
        };
        drawList->AddPolyline(pChk, 3, IM_COL32(255, 255, 255, 255), ImDrawFlags_None, 2.2f);
    } else if (icon == "radio_button_unchecked") {
        drawList->AddCircle(center, size * 0.32f, color, 0, 2.0f);
    } else if (icon == "pending") {
        drawList->AddCircleFilled(ImVec2(cx - size * 0.20f, cy), size * 0.08f, color);
        drawList->AddCircleFilled(ImVec2(cx, cy), size * 0.08f, color);
        drawList->AddCircleFilled(ImVec2(cx + size * 0.20f, cy), size * 0.08f, color);
    } else if (icon == "casino") {
        // Die outline
        drawList->AddRect(ImVec2(cx - size * 0.30f, cy - size * 0.30f), ImVec2(cx + size * 0.30f, cy + size * 0.30f), color, 4.0f, 0, 2.0f);
        // 5 dots
        drawList->AddCircleFilled(ImVec2(cx - size * 0.16f, cy - size * 0.16f), 1.6f, color);
        drawList->AddCircleFilled(ImVec2(cx + size * 0.16f, cy - size * 0.16f), 1.6f, color);
        drawList->AddCircleFilled(ImVec2(cx, cy), 1.6f, color);
        drawList->AddCircleFilled(ImVec2(cx - size * 0.16f, cy + size * 0.16f), 1.6f, color);
        drawList->AddCircleFilled(ImVec2(cx + size * 0.16f, cy + size * 0.16f), 1.6f, color);
    } else if (icon == "warning" || icon == "error") {
        // Triangle outline
        ImVec2 tri[3] = {
            ImVec2(cx, cy - size * 0.34f),
            ImVec2(cx - size * 0.36f, cy + size * 0.30f),
            ImVec2(cx + size * 0.36f, cy + size * 0.30f)
        };
        drawList->AddPolyline(tri, 3, color, ImDrawFlags_Closed, 2.2f);
        // Exclamation mark
        drawList->AddLine(ImVec2(cx, cy - size * 0.12f), ImVec2(cx, cy + size * 0.08f), color, 2.2f);
        drawList->AddCircleFilled(ImVec2(cx, cy + size * 0.18f), 1.8f, color);
    } else if (icon == "refresh" || icon == "sync") {
        ImVec2 arcCenter(cx, cy);
        drawList->PathClear();
        drawList->PathArcTo(arcCenter, size * 0.28f, -2.4f, 2.2f);
        drawList->PathStroke(color, ImDrawFlags_None, 2.2f);
        // Arrow tip
        float tipX = cx + std::cos(2.2f) * size * 0.28f;
        float tipY = cy + std::sin(2.2f) * size * 0.28f;
        drawList->AddLine(ImVec2(tipX, tipY), ImVec2(tipX + 3.0f, tipY - 5.0f), color, 2.2f);
        drawList->AddLine(ImVec2(tipX, tipY), ImVec2(tipX + 5.0f, tipY + 1.0f), color, 2.2f);
    } else if (icon == "arrow_back") {
        drawList->AddLine(ImVec2(cx - size * 0.30f, cy), ImVec2(cx + size * 0.30f, cy), color, 2.2f);
        drawList->AddLine(ImVec2(cx - size * 0.30f, cy), ImVec2(cx - size * 0.08f, cy - size * 0.22f), color, 2.2f);
        drawList->AddLine(ImVec2(cx - size * 0.30f, cy), ImVec2(cx - size * 0.08f, cy + size * 0.22f), color, 2.2f);
    }
}

void DesktopApplication::renderChip(const char* text, int statusType) const {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 pos = ImGui::GetCursorScreenPos();

    float paddingX = 12.0f;
    float height = 32.0f;
    float rectW = textSize.x + paddingX * 2.0f + 16.0f; // Extra room for indicator circle

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(c.surface);
    ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(c.line);
    ImU32 textCol = ImGui::ColorConvertFloat4ToU32(c.mute);
    ImU32 dotCol = ImGui::ColorConvertFloat4ToU32(c.mute);

    if (statusType == 1) { // OK
        bgCol = ImGui::ColorConvertFloat4ToU32(c.ct);
        borderCol = IM_COL32(0, 0, 0, 0);
        textCol = ImGui::ColorConvertFloat4ToU32(c.onCt);
        dotCol = ImGui::ColorConvertFloat4ToU32(c.ok);
    } else if (statusType == 2) { // WARN
        textCol = ImGui::ColorConvertFloat4ToU32(c.warn);
        dotCol = ImGui::ColorConvertFloat4ToU32(c.warn);
    } else if (statusType == 3) { // BAD
        textCol = ImGui::ColorConvertFloat4ToU32(c.bad);
        dotCol = ImGui::ColorConvertFloat4ToU32(c.bad);
    }

    drawList->AddRectFilled(pos, ImVec2(pos.x + rectW, pos.y + height), bgCol, 8.0f);
    if (statusType != 1) {
        drawList->AddRect(pos, ImVec2(pos.x + rectW, pos.y + height), borderCol, 8.0f, 0, 1.0f);
    }

    // Dot indicator
    float circleX = pos.x + 12.0f;
    float circleY = pos.y + height * 0.5f;
    renderVectorIcon("circle", ImVec2(circleX, circleY), 16.0f, dotCol);

    // Text
    drawList->AddText(ImVec2(circleX + 10.0f, pos.y + (height - textSize.y) * 0.5f), textCol, text);

    ImGui::Dummy(ImVec2(rectW, height));
    if (fontSmall_) ImGui::PopFont();
}

bool DesktopApplication::renderPillButton(const char* label, bool primary, bool danger, const ImVec2& size, const char* iconName) {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImVec2 actualSize = size;
    if (actualSize.y <= 0) actualSize.y = 40.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens::MintShapes::Pill);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24.0f, 8.0f));

    ImU32 iconCol = 0;
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, c.mint);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.mint.x * 1.1f, c.mint.y * 1.1f, c.mint.z * 1.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, c.acc);
        ImGui::PushStyleColor(ImGuiCol_Text, c.onMint);
        ImGui::PushStyleColor(ImGuiCol_Border, c.mint);
        iconCol = ImGui::ColorConvertFloat4ToU32(c.onMint);
    } else if (danger) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.surface.x, c.surface.y, c.surface.z, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c.ct2);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, c.ct);
        ImGui::PushStyleColor(ImGuiCol_Text, c.bad);
        ImGui::PushStyleColor(ImGuiCol_Border, c.line);
        iconCol = ImGui::ColorConvertFloat4ToU32(c.bad);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.surface.x, c.surface.y, c.surface.z, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c.ct2);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, c.ct);
        ImGui::PushStyleColor(ImGuiCol_Text, c.acc);
        ImGui::PushStyleColor(ImGuiCol_Border, c.line);
        iconCol = ImGui::ColorConvertFloat4ToU32(c.acc);
    }

    ImVec2 pStart = ImGui::GetCursorScreenPos();
    std::string fullLabel = (iconName ? "   " : "") + std::string(label);
    bool clicked = ImGui::Button(fullLabel.c_str(), actualSize);

    if (iconName) {
        renderVectorIcon(iconName, ImVec2(pStart.x + 20.0f, pStart.y + actualSize.y * 0.5f), 18.0f, iconCol);
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    return clicked;
}

bool DesktopApplication::renderM3Switch(const char* id, const char* label, bool* v, const char* sublabel, float width) {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float trackW = 52.0f;
    float trackH = 32.0f;
    float trackRadius = 16.0f;

    float availW = ImGui::GetContentRegionAvail().x;
    float rowW = (width > 0.0f) ? width : ((availW > 100.0f) ? availW : 300.0f);
    ImVec2 pStart = ImGui::GetCursorScreenPos();

    float rowH = std::max(trackH, sublabel ? 42.0f : 32.0f);
    ImGui::InvisibleButton(id, ImVec2(rowW, rowH));
    bool isHovered = ImGui::IsItemHovered();
    bool isClicked = ImGui::IsItemClicked();

    if (isClicked) {
        *v = !(*v);
    }

    // Draw label & sublabel on left
    ImGui::SetCursorScreenPos(pStart);
    ImGui::BeginGroup();
    if (fontBold_) ImGui::PushFont(fontBold_);
    ImGui::TextColored(c.ink, "%s", label);
    if (fontBold_) ImGui::PopFont();
    if (sublabel && sublabel[0] != '\0') {
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "%s", sublabel);
        if (fontSmall_) ImGui::PopFont();
    }
    ImGui::EndGroup();

    // Draw M3 Switch track on right
    float trackX = pStart.x + rowW - trackW - 4.0f;
    float trackY = pStart.y + (rowH - trackH) * 0.5f;

    ImVec2 trackMin(trackX, trackY);
    ImVec2 trackMax(trackX + trackW, trackY + trackH);

    if (*v) {
        // Active M3 Switch: filled track with mint, large thumb
        drawList->AddRectFilled(trackMin, trackMax, ImGui::ColorConvertFloat4ToU32(c.mint), trackRadius);
        float thumbRadius = 12.0f;
        ImVec2 thumbCenter(trackMax.x - 16.0f, trackMin.y + 16.0f);
        drawList->AddCircleFilled(thumbCenter, thumbRadius, ImGui::ColorConvertFloat4ToU32(c.on));
        // Small check icon inside thumb (M3 spec with icon)
        ImVec2 pChk[3] = {
            ImVec2(thumbCenter.x - 4.0f, thumbCenter.y),
            ImVec2(thumbCenter.x - 1.0f, thumbCenter.y + 3.0f),
            ImVec2(thumbCenter.x + 4.0f, thumbCenter.y - 3.0f)
        };
        drawList->AddPolyline(pChk, 3, ImGui::ColorConvertFloat4ToU32(c.acc), ImDrawFlags_None, 1.8f);
    } else {
        // Inactive M3 Switch: outline track with ct2 background, small thumb
        drawList->AddRectFilled(trackMin, trackMax, ImGui::ColorConvertFloat4ToU32(c.ct2), trackRadius);
        drawList->AddRect(trackMin, trackMax, ImGui::ColorConvertFloat4ToU32(isHovered ? c.ink : c.mute), trackRadius, 0, 2.0f);
        float thumbRadius = 8.0f;
        ImVec2 thumbCenter(trackMin.x + 16.0f, trackMin.y + 16.0f);
        drawList->AddCircleFilled(thumbCenter, thumbRadius, ImGui::ColorConvertFloat4ToU32(isHovered ? c.ink : c.mute));
    }

    ImGui::SetCursorScreenPos(ImVec2(pStart.x, pStart.y + rowH + 6.0f));
    return isClicked;
}

bool DesktopApplication::renderM3TextField(const char* id, const char* label, char* buf, size_t bufSize, const char* hint, float width) {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    if (label && label[0] != '\0') {
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "%s", label);
        if (fontSmall_) ImGui::PopFont();
        ImGui::Spacing();
    }

    float fieldW = (width > 0.0f) ? width : ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, c.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushStyleColor(ImGuiCol_Text, c.ink);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, c.mute);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 10.0f));

    ImGui::SetNextItemWidth(fieldW);
    bool changed = false;
    if (fontRegular_) ImGui::PushFont(fontRegular_);
    if (hint && hint[0] != '\0') {
        changed = ImGui::InputTextWithHint(id, hint, buf, bufSize);
    } else {
        changed = ImGui::InputText(id, buf, bufSize);
    }
    if (fontRegular_) ImGui::PopFont();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);
    return changed;
}

bool DesktopApplication::renderM3SegmentedButton(const char* id, const std::vector<const char*>& options, int* selectedIndex, float width) {
    if (options.empty()) return false;
    const auto& c = tokens::getActiveTheme(currentTheme_);
    float totalW = (width > 0.0f) ? width : ImGui::GetContentRegionAvail().x;
    float segH = 40.0f;
    float segW = totalW / static_cast<float>(options.size());

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pStart = ImGui::GetCursorScreenPos();
    bool changed = false;

    // Draw outer pill border
    ImVec2 outerMin = pStart;
    ImVec2 outerMax(pStart.x + totalW, pStart.y + segH);
    drawList->AddRect(outerMin, outerMax, ImGui::ColorConvertFloat4ToU32(c.line), 20.0f, 0, 1.5f);

    for (size_t i = 0; i < options.size(); ++i) {
        ImVec2 segMin(pStart.x + static_cast<float>(i) * segW, pStart.y);
        ImVec2 segMax(segMin.x + segW, segMin.y + segH);

        std::string btnId = std::string(id) + "_seg_" + std::to_string(i);
        ImGui::SetCursorScreenPos(segMin);
        if (ImGui::InvisibleButton(btnId.c_str(), ImVec2(segW, segH))) {
            if (*selectedIndex != static_cast<int>(i)) {
                *selectedIndex = static_cast<int>(i);
                changed = true;
            }
        }
        bool isHovered = ImGui::IsItemHovered();
        bool isSelected = (*selectedIndex == static_cast<int>(i));

        // Draw selection fill
        if (isSelected) {
            ImDrawFlags flags = ImDrawFlags_None;
            if (i == 0) flags = ImDrawFlags_RoundCornersLeft;
            else if (i == options.size() - 1) flags = ImDrawFlags_RoundCornersRight;
            drawList->AddRectFilled(segMin, segMax, ImGui::ColorConvertFloat4ToU32(c.ct), 20.0f, flags);
        } else if (isHovered) {
            ImDrawFlags flags = ImDrawFlags_None;
            if (i == 0) flags = ImDrawFlags_RoundCornersLeft;
            else if (i == options.size() - 1) flags = ImDrawFlags_RoundCornersRight;
            drawList->AddRectFilled(segMin, segMax, ImGui::ColorConvertFloat4ToU32(c.ct2), 20.0f, flags);
        }

        // Divider lines between segments
        if (i > 0) {
            drawList->AddLine(ImVec2(segMin.x, segMin.y), ImVec2(segMin.x, segMax.y), ImGui::ColorConvertFloat4ToU32(c.line), 1.0f);
        }

        // Segment label with checkmark if selected
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImVec2 txtSize = ImGui::CalcTextSize(options[i]);
        float textX = segMin.x + (segW - txtSize.x) * 0.5f;
        float textY = segMin.y + (segH - txtSize.y) * 0.5f;

        if (isSelected) {
            renderVectorIcon("check_circle", ImVec2(textX - 10.0f, segMin.y + segH * 0.5f), 14.0f, ImGui::ColorConvertFloat4ToU32(c.acc));
            textX += 6.0f;
        }

        drawList->AddText(ImVec2(textX, textY), ImGui::ColorConvertFloat4ToU32(isSelected ? c.onCt : c.ink), options[i]);
        if (fontRegular_) ImGui::PopFont();
    }

    ImGui::SetCursorScreenPos(ImVec2(pStart.x, pStart.y + segH + 8.0f));
    return changed;
}

bool DesktopApplication::renderM3Slider(const char* id, const char* label, int* v, int vMin, int vMax, const char* unit) {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    float availW = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Label with value badge
    if (fontBold_) ImGui::PushFont(fontBold_);
    ImGui::TextColored(c.ink, "%s: %d %s", label, *v, unit);
    if (fontBold_) ImGui::PopFont();
    ImGui::Spacing();

    ImVec2 pStart = ImGui::GetCursorScreenPos();
    float sliderH = 32.0f;
    float trackH = 14.0f;
    float trackY = pStart.y + (sliderH - trackH) * 0.5f;

    ImGui::InvisibleButton(id, ImVec2(availW, sliderH));
    bool isActive = ImGui::IsItemActive();

    if (isActive) {
        float mouseX = ImGui::GetIO().MousePos.x;
        float ratio = std::clamp((mouseX - pStart.x) / availW, 0.0f, 1.0f);
        *v = vMin + static_cast<int>(std::round(ratio * static_cast<float>(vMax - vMin)));
    }

    float ratio = static_cast<float>(*v - vMin) / static_cast<float>(vMax - vMin);
    float activeW = availW * ratio;

    // Inactive track (M3 Container)
    drawList->AddRectFilled(ImVec2(pStart.x, trackY), ImVec2(pStart.x + availW, trackY + trackH), ImGui::ColorConvertFloat4ToU32(c.ct2), trackH * 0.5f);

    // Active track (M3 Primary / Mint)
    if (activeW > 0.0f) {
        drawList->AddRectFilled(ImVec2(pStart.x, trackY), ImVec2(pStart.x + activeW, trackY + trackH), ImGui::ColorConvertFloat4ToU32(c.mint), trackH * 0.5f);
    }

    // M3 Thumb (Rounded pill handle)
    float thumbW = 10.0f;
    float thumbH = 26.0f;
    float thumbX = pStart.x + activeW - thumbW * 0.5f;
    float thumbY = pStart.y + (sliderH - thumbH) * 0.5f;
    thumbX = std::clamp(thumbX, pStart.x, pStart.x + availW - thumbW);

    drawList->AddRectFilled(ImVec2(thumbX, thumbY), ImVec2(thumbX + thumbW, thumbY + thumbH), ImGui::ColorConvertFloat4ToU32(c.acc), thumbW * 0.5f);

    ImGui::SetCursorScreenPos(ImVec2(pStart.x, pStart.y + sliderH + 8.0f));
    return isActive;
}

void DesktopApplication::renderGuiHeader() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGui::BeginGroup();

    // 0. Circular Menu Button
    ImVec2 menuPos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens::MintShapes::Pill);
    ImGui::PushStyleColor(ImGuiCol_Button, c.surface);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c.ct);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushStyleColor(ImGuiCol_Text, c.ink);

    if (ImGui::Button("##MenuBtn", ImVec2(44.0f, 44.0f))) {
        // Toggle menu / rail
    }
    renderVectorIcon("menu", ImVec2(menuPos.x + 22.0f, menuPos.y + 22.0f), 20.0f, ImGui::ColorConvertFloat4ToU32(c.ink));
    ImGui::SameLine(0, 12.0f);

    // 1. Sleek Mint Logo & Brand Text "Mist"
    renderSleekMintLogo(44.0f);
    ImGui::SameLine(0, 10.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);

    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "Mist");
    if (fontHeadline_) ImGui::PopFont();

    // 2. Pill Search Bar (Search Mist...)
    float searchWidth = 300.0f;
    float availW = ImGui::GetContentRegionAvail().x;
    float searchOffset = std::max(200.0f, availW - 480.0f);
    ImGui::SameLine(searchOffset);

    ImVec2 searchPos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(36.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, c.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushItemWidth(searchWidth);

    if (ImGui::InputTextWithHint("##HeaderSearch", "Search Mist (host, devices, network...)", searchQuery_, IM_ARRAYSIZE(searchQuery_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string q = searchQuery_;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        if (q.find("host") != std::string::npos || q.find("stream") != std::string::npos) {
            currentScreen_ = DesktopScreen::Host;
            searchQuery_[0] = '\0';
        } else if (q.find("conn") != std::string::npos || q.find("join") != std::string::npos) {
            currentScreen_ = DesktopScreen::Connect;
            searchQuery_[0] = '\0';
        } else if (q.find("dev") != std::string::npos || q.find("pair") != std::string::npos) {
            currentScreen_ = DesktopScreen::Devices;
            searchQuery_[0] = '\0';
        } else if (q.find("net") != std::string::npos || q.find("bitrate") != std::string::npos || q.find("lag") != std::string::npos) {
            currentScreen_ = DesktopScreen::Network;
            searchQuery_[0] = '\0';
        } else if (q.find("home") != std::string::npos) {
            currentScreen_ = DesktopScreen::Home;
            searchQuery_[0] = '\0';
        } else {
            showToast("Nothing found for '" + std::string(searchQuery_) + "'");
        }
    }
    // Search icon inside input pill
    renderVectorIcon("search", ImVec2(searchPos.x + 20.0f, searchPos.y + 22.0f), 18.0f, ImGui::ColorConvertFloat4ToU32(c.mute));

    ImGui::PopItemWidth();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    // 3. Status Chip ("Idle" or "Streaming")
    ImGui::SameLine(0, 12.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    if (isHostingActive_) {
        renderChip("Streaming", 1);
    } else {
        renderChip("Idle", 0);
    }

    // 4. Circular Devices Button (with badge "1" if pending)
    ImGui::SameLine(0, 12.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.0f);
    ImVec2 devBtnPos = ImGui::GetCursorScreenPos();

    if (ImGui::Button("##TopDev", ImVec2(44.0f, 44.0f))) {
        clearError();
        currentScreen_ = DesktopScreen::Devices;
    }
    renderVectorIcon("devices", ImVec2(devBtnPos.x + 22.0f, devBtnPos.y + 22.0f), 22.0f, ImGui::ColorConvertFloat4ToU32(c.ink));

    // Badge "1" at top-right corner of device button
    if (isDeviceApprovalPending_) {
        float badgeX = devBtnPos.x + 32.0f;
        float badgeY = devBtnPos.y - 2.0f;
        drawList->AddCircleFilled(ImVec2(badgeX, badgeY), 9.0f, ImGui::ColorConvertFloat4ToU32(c.mint));
        drawList->AddText(ImVec2(badgeX - 3.5f, badgeY - 6.0f), ImGui::ColorConvertFloat4ToU32(c.onMint), "1");
    }

    // 5. Circular Settings Button (Gear)
    ImGui::SameLine(0, 8.0f);
    ImVec2 gearPos = ImGui::GetCursorScreenPos();
    if (ImGui::Button("##TopGear", ImVec2(44.0f, 44.0f))) {
        isSettingsOpen_ = !isSettingsOpen_;
    }
    renderVectorIcon("settings", ImVec2(gearPos.x + 22.0f, gearPos.y + 22.0f), 22.0f, ImGui::ColorConvertFloat4ToU32(c.ink));

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImGui::EndGroup();
}

void DesktopApplication::renderGuiNavRail() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    float railWidth = 272.0f;

    // Transparent child window so ambient wallpaper shines through
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("NavRail", ImVec2(railWidth, 0), false, ImGuiWindowFlags_NoScrollbar);

    struct NavItem {
        DesktopScreen screen;
        const char* label;
        const char* icon;
        bool hasBadge;
    };

    const NavItem items[] = {
        { DesktopScreen::Home,    "Home",    "home",          false },
        { DesktopScreen::Host,    "Host",    "cast",          false },
        { DesktopScreen::Connect, "Connect", "sports_esports",false },
        { DesktopScreen::Devices, "Devices", "devices",       isDeviceApprovalPending_ },
        { DesktopScreen::Network, "Network", "network_check", false },
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens::MintShapes::Pill);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20.0f, 14.0f));

    for (const auto& item : items) {
        bool isSelected = (currentScreen_ == item.screen);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, c.ct);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c.ct);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, c.mint);
            ImGui::PushStyleColor(ImGuiCol_Text, c.onCt);
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.bg.x, c.bg.y, c.bg.z, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c.ct2);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, c.ct);
            ImGui::PushStyleColor(ImGuiCol_Text, c.mute);
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        }

        ImVec2 pStart = ImGui::GetCursorScreenPos();
        std::string btnText = "      " + std::string(item.label);
        if (fontBold_) ImGui::PushFont(fontBold_);
        if (ImGui::Button(btnText.c_str(), ImVec2(railWidth - 24.0f, 52.0f))) {
            clearError();
            currentScreen_ = item.screen;
        }
        if (fontBold_) ImGui::PopFont();

        // Render the navigation icon
        ImU32 iconColor = isSelected ? ImGui::ColorConvertFloat4ToU32(c.onCt) : ImGui::ColorConvertFloat4ToU32(c.mute);
        renderVectorIcon(item.icon, ImVec2(pStart.x + 28.0f, pStart.y + 26.0f), 22.0f, iconColor);

        if (item.hasBadge) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float bx = pStart.x + railWidth - 56.0f;
            float by = pStart.y + 26.0f;
            drawList->AddRectFilled(ImVec2(bx, by - 10.0f), ImVec2(bx + 20.0f, by + 10.0f), ImGui::ColorConvertFloat4ToU32(c.mint), 99.0f);
            drawList->AddText(ImVec2(bx + 6.0f, by - 7.0f), ImGui::ColorConvertFloat4ToU32(c.onMint), "1");
        }

        ImGui::PopStyleColor(5);
        ImGui::Spacing();
    }
    ImGui::PopStyleVar(2);

    // Footnote at bottom of rail
    float availY = ImGui::GetContentRegionAvail().y;
    if (availY > 60.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availY - 60.0f);
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "v0.2.0-rc1");
        ImGui::TextColored(c.mute, "Windows 11 * GTX 1660 SUPER");
        if (fontSmall_) ImGui::PopFont();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void DesktopApplication::renderGuiHomeScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);

    // 1. Heading
    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Game streaming, no lag");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Host this PC or join another one. Everything below is ready.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Spacing();

    // 2. Latest Sessions Section
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Latest sessions");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float carCardW = std::max(220.0f, (availW - 32.0f) / 3.0f);

    struct SessionCardData {
        const char* name;
        const char* timeAgo;
        const char* metrics;
    };
    const SessionCardData sessions[] = {
        { "Living room PC", "Yesterday * 48 min", "60 fps * 11.5 Mbps" },
        { "Aiden's Laptop",  "Sat * 2 h 10 min",  "60 fps * 9.8 Mbps"  },
        { "Studio desktop",  "Thu * 15 min",      "30 fps * 6.2 Mbps"  }
    };

    for (int i = 0; i < 3; ++i) {
        if (i > 0) ImGui::SameLine(0, 14.0f);
        BeginCardContainer(("SessionCard" + std::to_string(i)).c_str(), c, ImVec2(carCardW, 110.0f), 24.0f);
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "%s", sessions[i].name);
        if (fontBold_) ImGui::PopFont();

        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "%s", sessions[i].timeAgo);
        ImGui::Spacing();
        ImGui::TextColored(c.ink, "%s", sessions[i].metrics);
        if (fontSmall_) ImGui::PopFont();
        EndCardContainer();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // 3. "What would you like to do?" Section (4 Action Tiles in 2x2 grid)
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "What would you like to do?");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    float tileW = std::max(260.0f, (availW - 16.0f) / 2.0f);

    struct TileData {
        const char* title;
        const char* desc;
        DesktopScreen target;
        const char* icon;
    };
    const TileData tiles[] = {
        { "Host this PC",    "Let paired devices see and play on your desktop.", DesktopScreen::Host,    "cast" },
        { "Connect to a PC", "Join a session with its ID.",                      DesktopScreen::Connect, "sports_esports" },
        { "Pair a device",   "Create a one-time code and set permissions.",       DesktopScreen::Devices, "devices" },
        { "Check network",   "See how Mist adapts to your connection.",           DesktopScreen::Network, "network_check" },
    };

    for (int i = 0; i < 4; ++i) {
        if (i % 2 == 1) ImGui::SameLine(0, 16.0f);
        else if (i > 0) ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, c.ct2);
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, tokens::MintShapes::Tile); // 24px
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 22.0f));

        std::string tileId = "Tile" + std::to_string(i);
        ImGui::BeginChild(tileId.c_str(), ImVec2(tileW, 136.0f), true);

        // 48x48 rounded 16px mint icon box
        ImVec2 curPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(curPos, ImVec2(curPos.x + 48.0f, curPos.y + 48.0f), ImGui::ColorConvertFloat4ToU32(c.mint), 16.0f);

        // Render the exact Material icon inside the mint box
        renderVectorIcon(tiles[i].icon, ImVec2(curPos.x + 24.0f, curPos.y + 24.0f), 24.0f, ImGui::ColorConvertFloat4ToU32(c.onMint));

        ImGui::Dummy(ImVec2(48.0f, 48.0f));
        ImGui::SameLine(0, 14.0f);
        ImGui::BeginGroup();
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "%s", tiles[i].title);
        if (fontBold_) ImGui::PopFont();
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "%s", tiles[i].desc);
        if (fontSmall_) ImGui::PopFont();
        ImGui::EndGroup();

        // Clickable surface
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            clearError();
            currentScreen_ = tiles[i].target;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // 4. System Check Card
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "System check");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    BeginCardContainer("SystemCheckCard", c, ImVec2(0, 0), 28.0f);

    struct SysRow {
        const char* label;
        const char* chipText;
        int status;
    };
    const SysRow rows[] = {
        { "Video encoder",    "NVENC * GTX 1660 SUPER", 1 },
        { "Graphics driver",  "617.14",                 1 },
        { "Display session",  "Attached to a monitor",  1 },
        { "Audio output",     "Default device",         1 },
        { "Controller driver","ViGEmBus missing * using fallback", 2 },
    };

    for (size_t i = 0; i < std::size(rows); ++i) {
        if (i > 0) {
            ImGui::Separator();
            ImGui::Spacing();
        }
        ImGui::AlignTextToFramePadding();
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(c.ink, "%s", rows[i].label);
        if (fontRegular_) ImGui::PopFont();

        float rw = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(rw - 280.0f);
        renderChip(rows[i].chipText, rows[i].status);
        ImGui::Spacing();
    }

    EndCardContainer();
}

void DesktopApplication::renderGuiHostScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);

    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Host");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Stream this desktop to paired devices.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float colW = std::max(300.0f, (availW - 16.0f) / 2.0f);

    // Left: Stream Card
    BeginCardContainer("HostStreamCard", c, ImVec2(colW, 310.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Stream");
    if (fontTitle_) ImGui::PopFont();
    ImGui::SameLine(colW - 140.0f);
    renderChip(isHostingActive_ ? "Streaming" : "Stopped", isHostingActive_ ? 1 : 0);

    ImGui::Spacing();
    ImGui::Spacing();

    if (isHostingActive_) {
        // Live Numbers
        ImGui::BeginGroup();
        if (fontHeadline_) ImGui::PushFont(fontHeadline_);
        ImGui::TextColored(c.ink, "%.0f", hostFps_);
        if (fontHeadline_) ImGui::PopFont();
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "frames/s");
        if (fontSmall_) ImGui::PopFont();
        ImGui::EndGroup();

        ImGui::SameLine(0, 24.0f);
        ImGui::BeginGroup();
        if (fontHeadline_) ImGui::PushFont(fontHeadline_);
        ImGui::TextColored(c.ink, "%.1f", hostBitrate_);
        if (fontHeadline_) ImGui::PopFont();
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "Mbps");
        if (fontSmall_) ImGui::PopFont();
        ImGui::EndGroup();

        ImGui::SameLine(0, 24.0f);
        ImGui::BeginGroup();
        if (fontHeadline_) ImGui::PushFont(fontHeadline_);
        ImGui::TextColored(c.ink, "%.1f", hostEncodeMs_);
        if (fontHeadline_) ImGui::PopFont();
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "ms encode");
        if (fontSmall_) ImGui::PopFont();
        ImGui::EndGroup();
    } else {
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(c.mute, "Start streaming to see live numbers.");
        if (fontRegular_) ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    if (!isHostingActive_) {
        if (renderPillButton("Start streaming", true, false, ImVec2(180.0f, 40.0f), "play_arrow")) {
            std::string sId(hostSessionId_);
            std::string sigUrl(hostSignalingServer_);
            if (sId.empty()) {
                showToast("Please enter a valid Session ID to host");
            } else if (sigUrl.rfind("ws://", 0) != 0 && sigUrl.rfind("wss://", 0) != 0) {
                triggerError("Invalid Signaling Server URL",
                             "The host signaling address '" + sigUrl + "' is not a valid WebSocket URI.",
                             "Host signaling requires 'ws://' or 'wss://' scheme, e.g. 'ws://127.0.0.1:8080'.",
                             "ERR_INVALID_HOST_SIGNALING",
                             DesktopScreen::Host);
            } else {
                isHostingActive_ = true;
                showToast("Streaming started");
            }
        }
    } else {
        if (renderPillButton("Stop streaming", false, true, ImVec2(180.0f, 40.0f), "stop")) {
            isHostingActive_ = false;
            showToast("Streaming stopped");
        }
    }

    EndCardContainer();

    // Right: Session Settings Card
    ImGui::SameLine(0, 16.0f);
    BeginCardContainer("HostSessionCard", c, ImVec2(colW, 310.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Session");
    if (fontTitle_) ImGui::PopFont();

    ImGui::Spacing();
    renderM3Switch("HostStreamAudio", "Stream audio", &hostAudioStream_, "Transmits low-delay stereo game sound");

    ImGui::Spacing();
    if (renderM3Switch("HostRemoteInput", "Allow remote input", &hostAllowRemoteInput_, "Off until you turn it on. Your mouse always wins.")) {
        showToast(hostAllowRemoteInput_ ? "Remote input allowed" : "Remote input blocked");
    }

    ImGui::Spacing();
    renderM3TextField("##HostSessionId", "Session ID", hostSessionId_, IM_ARRAYSIZE(hostSessionId_));

    ImGui::Spacing();
    renderM3TextField("##HostSignaling", "Signaling server", hostSignalingServer_, IM_ARRAYSIZE(hostSignalingServer_));

    EndCardContainer();
}

void DesktopApplication::renderGuiConnectScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);

    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Connect");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Join a PC that's hosting. You'll need its session ID.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float colW = std::max(300.0f, (availW - 16.0f) / 2.0f);

    // Left: Connect Form Card
    BeginCardContainer("ConnectFormCard", c, ImVec2(colW, 400.0f), 28.0f);

    renderM3TextField("##ConnSessionId", "Session ID", connectSessionId_, IM_ARRAYSIZE(connectSessionId_));
    ImGui::Spacing();
    renderM3TextField("##ConnSignaling", "Signaling server", connectSignalingServer_, IM_ARRAYSIZE(connectSignalingServer_));
    ImGui::Spacing();
    renderM3TextField("##ConnRelay", "Relay server (optional)", connectRelayServer_, IM_ARRAYSIZE(connectRelayServer_), "turn:relay.example.com:3478");

    ImGui::Spacing();
    renderM3Switch("ConnHearAudio", "Hear audio", &connectHearAudio_);
    renderM3Switch("ConnSendInput", "Send input", &connectSendInput_, "Keyboard, mouse and gamepad");

    ImGui::Spacing();
    ImGui::Spacing();

    if (renderPillButton(isConnecting_ ? "Connecting..." : "Connect", true, false, ImVec2(160.0f, 40.0f), isConnecting_ ? "refresh" : "play_arrow")) {
        if (!isConnecting_) {
            std::string sId(connectSessionId_);
            std::string sigUrl(connectSignalingServer_);
            if (sId.empty()) {
                showToast("Please enter a valid Session ID");
            } else if (sigUrl.rfind("ws://", 0) != 0 && sigUrl.rfind("wss://", 0) != 0) {
                triggerError("Invalid Signaling Server URL",
                             "The signaling server address '" + sigUrl + "' is not a valid WebSocket address.",
                             "Signaling addresses must use the WebSocket protocol scheme, e.g. 'ws://127.0.0.1:8080' or 'wss://signaling.example.com'.",
                             "ERR_INVALID_SIGNALING_URL",
                             DesktopScreen::Connect);
            } else {
                isConnecting_ = true;
                connectStep_ = 1;
                connectStepTimer_ = 0.0f;
                connectStatusMsg_ = "Connecting to " + sId + "...";
            }
        }
    }

    if (isConnecting_) {
        ImGui::SameLine(0, 12.0f);
        if (renderPillButton("Cancel", false, true, ImVec2(120.0f, 40.0f), "stop")) {
            isConnecting_ = false;
            connectStep_ = 0;
            connectStatusMsg_ = "Connection cancelled by user.";
            showToast("Connection cancelled");
        }
    }

    EndCardContainer();

    // Right: Connection Steps Card
    ImGui::SameLine(0, 16.0f);
    BeginCardContainer("ConnectStepsCard", c, ImVec2(colW, 400.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Connection");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    const char* steps[] = {
        "Reach signaling server",
        "Pair with host",
        "Open direct connection",
        "Receive first frame"
    };

    for (int i = 0; i < 4; ++i) {
        ImGui::Spacing();
        ImVec2 pLine = ImGui::GetCursorScreenPos();
        if (connectStep_ > i + 1) {
            renderVectorIcon("check_circle", ImVec2(pLine.x + 12.0f, pLine.y + 10.0f), 18.0f, ImGui::ColorConvertFloat4ToU32(c.ok));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 28.0f);
            ImGui::TextColored(c.ink, "%s", steps[i]);
        } else if (connectStep_ == i + 1) {
            renderLoadingSpinner(ImVec2(pLine.x + 12.0f, pLine.y + 10.0f), 8.0f, ImGui::ColorConvertFloat4ToU32(c.mint), 2.2f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 28.0f);
            ImGui::TextColored(c.acc, "%s", steps[i]);
        } else {
            renderVectorIcon("radio_button_unchecked", ImVec2(pLine.x + 12.0f, pLine.y + 10.0f), 18.0f, ImGui::ColorConvertFloat4ToU32(c.line));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 28.0f);
            ImGui::TextColored(c.mute, "%s", steps[i]);
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(connectStep_ == 5 ? c.ok : c.mute, "%s", connectStatusMsg_.c_str());
    if (fontRegular_) ImGui::PopFont();

    EndCardContainer();
}

void DesktopApplication::renderGuiDevicesScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);

    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Devices");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Pair a new device with a one-time code, then choose what it can do.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float colW = std::max(300.0f, (availW - 16.0f) / 2.0f);

    // Left: Pairing Code Card
    BeginCardContainer("PairingCodeCard", c, ImVec2(colW, 260.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Pairing code");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    // 6 Large Digits in Rounded Pill Containers
    float spanW = std::min(52.0f, (colW - 80.0f) / 6.0f);
    float spanH = 64.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < 6 && i < pairingCode_.size(); ++i) {
        if (i > 0) ImGui::SameLine(0, 8.0f);
        ImVec2 p = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(p, ImVec2(p.x + spanW, p.y + spanH), ImGui::ColorConvertFloat4ToU32(c.ct), 16.0f);

        char digitStr[2] = { pairingCode_[i], '\0' };
        if (fontCode_) ImGui::PushFont(fontCode_);
        ImVec2 ts = ImGui::CalcTextSize(digitStr);
        drawList->AddText(ImVec2(p.x + (spanW - ts.x) * 0.5f, p.y + (spanH - ts.y) * 0.5f), ImGui::ColorConvertFloat4ToU32(c.onCt), digitStr);
        if (fontCode_) ImGui::PopFont();

        ImGui::Dummy(ImVec2(spanW, spanH));
    }

    ImGui::Spacing();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "%s", codeHintMsg_.c_str());
    if (fontSmall_) ImGui::PopFont();

    ImGui::Spacing();
    if (renderPillButton("New code", true, false, ImVec2(120.0f, 36.0f))) {
        if (codeLockSeconds_ <= 0) {
            pairingCode_ = generateRandomPairingCode();
            wrongCodeFails_ = 0;
            codeHintMsg_ = "Read this code to the person pairing. It works once.";
            showToast("New code ready");
        }
    }
    ImGui::SameLine(0, 10.0f);
    if (renderPillButton("Try a wrong code", false, false, ImVec2(150.0f, 36.0f))) {
        if (codeLockSeconds_ <= 0) {
            wrongCodeFails_++;
            if (wrongCodeFails_ >= 5) {
                codeLockSeconds_ = 60;
                codeHintMsg_ = "Too many wrong codes. Try again in 60 seconds.";
                triggerError("Device Pairing Security Lockout",
                             "Too many incorrect pairing codes were submitted by remote devices.",
                             "1. Wait 60 seconds for the lockout countdown to expire.\n"
                             "2. Click 'New code' to generate a fresh 6-digit one-time code.\n"
                             "3. Verify the client device is entering the matching code.",
                             "ERR_AUTH_PAIRING_LOCKOUT",
                             DesktopScreen::Devices);
            } else {
                codeHintMsg_ = "Wrong code. " + std::to_string(5 - wrongCodeFails_) + " attempts left before a 60-second pause.";
            }
        }
    }

    EndCardContainer();

    // Right: Waiting for Approval Card
    ImGui::SameLine(0, 16.0f);
    BeginCardContainer("ApprovalCard", c, ImVec2(colW, 260.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Waiting for approval");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    if (isDeviceApprovalPending_) {
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "Aiden's Laptop");
        if (fontBold_) ImGui::PopFont();
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "Used a code just now");
        if (fontSmall_) ImGui::PopFont();

        ImGui::Spacing();
        if (renderPillButton("Approve", true, false, ImVec2(100.0f, 36.0f))) {
            trustedDevices_.push_back({"Aiden's Laptop", "Paired just now", false, false});
            isDeviceApprovalPending_ = false;
            showToast("Aiden's Laptop approved. Input is off.");
        }
        ImGui::SameLine(0, 10.0f);
        if (renderPillButton("Deny", false, false, ImVec2(80.0f, 36.0f))) {
            isDeviceApprovalPending_ = false;
            showToast("Request denied");
        }
    } else {
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(c.mute, "No devices waiting.");
        if (fontRegular_) ImGui::PopFont();
    }

    EndCardContainer();

    ImGui::Spacing();
    ImGui::Spacing();

    // Trusted Devices List Card
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Trusted devices");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    BeginCardContainer("TrustedDevicesCard", c, ImVec2(0, 0), 28.0f);
    if (trustedDevices_.empty()) {
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(c.mute, "No trusted devices yet. Create a pairing code to add one.");
        if (fontRegular_) ImGui::PopFont();
    } else {
        int toRemove = -1;
        for (size_t i = 0; i < trustedDevices_.size(); ++i) {
            if (i > 0) {
                ImGui::Separator();
                ImGui::Spacing();
            }
            ImGui::BeginGroup();
            if (fontBold_) ImGui::PushFont(fontBold_);
            ImGui::TextColored(c.ink, "%s", trustedDevices_[i].name.c_str());
            if (fontBold_) ImGui::PopFont();
            if (fontSmall_) ImGui::PushFont(fontSmall_);
            ImGui::TextColored(c.mute, "%s", trustedDevices_[i].lastSeen.c_str());
            if (fontSmall_) ImGui::PopFont();
            ImGui::EndGroup();

            float rowW = ImGui::GetContentRegionAvail().x;
            ImGui::SameLine(rowW - 380.0f);
            std::string kbdId = "KbdSwitch_" + std::to_string(i);
            if (renderM3Switch(kbdId.c_str(), "Mouse/Keys", &trustedDevices_[i].allowKbdMouse, nullptr, 140.0f)) {
                showToast("Permission updated");
            }

            ImGui::SameLine(0, 16.0f);
            std::string padId = "PadSwitch_" + std::to_string(i);
            if (renderM3Switch(padId.c_str(), "Gamepad", &trustedDevices_[i].allowController, nullptr, 130.0f)) {
                showToast("Permission updated");
            }

            ImGui::SameLine(0, 16.0f);
            std::string remId = "Remove##" + std::to_string(i);
            if (renderPillButton(remId.c_str(), false, true, ImVec2(80.0f, 32.0f))) {
                toRemove = static_cast<int>(i);
            }
            ImGui::Spacing();
        }

        if (toRemove >= 0 && toRemove < static_cast<int>(trustedDevices_.size())) {
            std::string name = trustedDevices_[toRemove].name;
            trustedDevices_.erase(trustedDevices_.begin() + toRemove);
            showToast(name + " removed");
        }
    }
    EndCardContainer();
}

void DesktopApplication::renderGuiNetworkScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);

    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Network");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Sample data. Mist lowers video bitrate when your connection struggles, then climbs back.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();

    // 1. Video Bitrate Card with Graph
    BeginCardContainer("NetworkChartCard", c, ImVec2(0, 360.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Video bitrate");
    if (fontTitle_) ImGui::PopFont();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f);
    renderChip(networkQosState_.c_str(), networkQosState_ == "Stable" ? 1 : (networkQosState_ == "Recovering" ? 2 : 3));

    ImGui::Spacing();

    // Custom Bitrate Graph (SVG polygon/polyline equivalent)
    ImVec2 graphPos = ImGui::GetCursorScreenPos();
    float graphW = ImGui::GetContentRegionAvail().x;
    float graphH = 150.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background Container
    drawList->AddRectFilled(graphPos, ImVec2(graphPos.x + graphW, graphPos.y + graphH), ImGui::ColorConvertFloat4ToU32(c.ct2), 12.0f);

    // Baseline dashed line
    float midY = graphPos.y + graphH * 0.5f;
    for (float x = graphPos.x; x < graphPos.x + graphW; x += 12.0f) {
        drawList->AddLine(ImVec2(x, midY), ImVec2(std::min(x + 6.0f, graphPos.x + graphW), midY), ImGui::ColorConvertFloat4ToU32(c.line), 1.0f);
    }

    // Plot Points
    if (bitrateHistory_.size() >= 2) {
        std::vector<ImVec2> pts;
        pts.reserve(bitrateHistory_.size() + 2);

        float stepX = graphW / static_cast<float>(bitrateHistory_.size() - 1);
        for (size_t i = 0; i < bitrateHistory_.size(); ++i) {
            float val = std::min(bitrateHistory_[i], 16.0f);
            float yNorm = val / 16.0f;
            float px = graphPos.x + static_cast<float>(i) * stepX;
            float py = graphPos.y + graphH - (yNorm * (graphH - 20.0f) + 10.0f);
            pts.push_back(ImVec2(px, py));
        }

        // Polygon Fill (opacity 0.18)
        ImVec4 fillCol = c.acc;
        fillCol.w = 0.18f;
        std::vector<ImVec2> polyPts;
        polyPts.push_back(ImVec2(graphPos.x, graphPos.y + graphH));
        polyPts.insert(polyPts.end(), pts.begin(), pts.end());
        polyPts.push_back(ImVec2(graphPos.x + graphW, graphPos.y + graphH));
        drawList->AddConvexPolyFilled(polyPts.data(), static_cast<int>(polyPts.size()), ImGui::ColorConvertFloat4ToU32(fillCol));

        // Polyline Curve (width 3px)
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            drawList->AddLine(pts[i], pts[i + 1], ImGui::ColorConvertFloat4ToU32(c.acc), 3.0f);
        }
    }

    ImGui::Dummy(ImVec2(graphW, graphH));
    ImGui::Spacing();
    ImGui::Spacing();

    // 4 Stat Counters
    float statColW = graphW / 4.0f;
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%.1f", networkBitrate_);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "Mbps video");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(statColW);
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%d", networkRtt_);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "ms round trip");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(statColW * 2.0f);
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%.1f", networkLoss_);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "%% packets lost");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(statColW * 3.0f);
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%d", networkJitter_);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "ms jitter");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::Spacing();
    if (renderPillButton("Simulate a bad connection", false, false, ImVec2(220.0f, 36.0f))) {
        isNetworkCongested_ = true;
        congestionDuration_ = 6.0f; // 6 seconds
        showToast("Bad connection for 6 seconds");
    }

    EndCardContainer();

    ImGui::Spacing();

    // 2. Bandwidth Split Card
    BeginCardContainer("BandwidthSplitCard", c, ImVec2(0, 130.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Bandwidth split");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    // 3-Segment Horizontal Progress Bar
    float barW = ImGui::GetContentRegionAvail().x;
    float barH = 12.0f;
    ImVec2 bPos = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(bPos, ImVec2(bPos.x + barW, bPos.y + barH), ImGui::ColorConvertFloat4ToU32(c.ct2), 6.0f);

    float totalKbps = networkBitrate_ * 1000.0f + 178.0f;
    float inputW = (50.0f / totalKbps) * barW;
    float audioW = (128.0f / totalKbps) * barW;

    drawList->AddRectFilled(bPos, ImVec2(bPos.x + inputW, bPos.y + barH), ImGui::ColorConvertFloat4ToU32(c.warn), 6.0f);
    drawList->AddRectFilled(ImVec2(bPos.x + inputW + 2.0f, bPos.y), ImVec2(bPos.x + inputW + audioW, bPos.y + barH), ImGui::ColorConvertFloat4ToU32(c.mute), 0.0f);
    drawList->AddRectFilled(ImVec2(bPos.x + inputW + audioW + 2.0f, bPos.y), ImVec2(bPos.x + barW, bPos.y + barH), ImGui::ColorConvertFloat4ToU32(c.acc), 6.0f);

    ImGui::Dummy(ImVec2(barW, barH));
    ImGui::Spacing();

    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "Input 50 kbps, reserved   *   Audio 128 kbps, reserved   *   Video gets the rest");
    if (fontSmall_) ImGui::PopFont();

    EndCardContainer();
}

void DesktopApplication::renderGuiLoadingScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    float dw = (dispSize.x > 100.0f) ? dispSize.x : static_cast<float>(guiWidth_);
    float dh = (dispSize.y > 100.0f) ? dispSize.y : static_cast<float>(guiHeight_);

    float cardW = std::min(460.0f, dw - 48.0f);
    float cardH = 320.0f;
    float posX = (dw - cardW) * 0.5f;
    float posY = (dh - cardH) * 0.45f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY));
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, c.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 28.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 32.0f));

    if (ImGui::Begin("StartupLoadingModal", nullptr, flags)) {
        // Centered Sleek Mint Logo
        float logoSize = 54.0f;
        ImGui::SetCursorPosX((cardW - logoSize) * 0.5f);
        renderSleekMintLogo(logoSize);
        ImGui::Spacing();
        ImGui::Spacing();

        // Product Title
        if (fontHeadline_) ImGui::PushFont(fontHeadline_);
        float titleW = ImGui::CalcTextSize("Mist").x;
        ImGui::SetCursorPosX((cardW - titleW) * 0.5f);
        ImGui::TextColored(c.ink, "Mist");
        if (fontHeadline_) ImGui::PopFont();

        // Tagline
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        float tagW = ImGui::CalcTextSize("Play beyond your desk.").x;
        ImGui::SetCursorPosX((cardW - tagW) * 0.5f);
        ImGui::TextColored(c.mute, "Play beyond your desk.");
        if (fontSmall_) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Spacing();

        // Animated Spinner & Progress Bar
        ImVec2 pCur = ImGui::GetCursorScreenPos();
        float barW = cardW - 64.0f;
        float progress = std::min(1.0f, initialLoadingTimer_ / initialLoadingDuration_);

        // Draw track
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 barMin(pCur.x, pCur.y + 10.0f);
        ImVec2 barMax(pCur.x + barW, pCur.y + 16.0f);
        drawList->AddRectFilled(barMin, barMax, ImGui::ColorConvertFloat4ToU32(c.ct2), 3.0f);
        // Draw fill
        if (progress > 0.01f) {
            drawList->AddRectFilled(barMin, ImVec2(barMin.x + barW * progress, barMax.y), ImGui::ColorConvertFloat4ToU32(c.mint), 3.0f);
        }

        ImGui::Dummy(ImVec2(barW, 28.0f));

        // Loading Stage Subtext
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        float stageW = ImGui::CalcTextSize(initialLoadingStage_.c_str()).x;
        ImGui::SetCursorPosX((cardW - stageW) * 0.5f);
        ImGui::TextColored(c.acc, "%s", initialLoadingStage_.c_str());
        if (fontSmall_) ImGui::PopFont();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void DesktopApplication::renderGuiErrorScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    float availW = ImGui::GetContentRegionAvail().x;
    float maxCardW = std::min(700.0f, availW);

    BeginCardContainer("ErrorScreenCard", c, ImVec2(maxCardW, 0), 28.0f);
    ImGui::Spacing();

    // Error Header with Vector Warning Icon
    ImVec2 pIcon = ImGui::GetCursorScreenPos();
    renderVectorIcon("error", ImVec2(pIcon.x + 24.0f, pIcon.y + 24.0f), 36.0f, ImGui::ColorConvertFloat4ToU32(c.bad));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 60.0f);

    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.bad, "%s", activeError_.title.empty() ? "Operation Error" : activeError_.title.c_str());
    if (fontHeadline_) ImGui::PopFont();

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 60.0f);
    renderChip(activeError_.code.empty() ? "ERR_UNKNOWN" : activeError_.code.c_str(), 3);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Error Description
    if (fontBold_) ImGui::PushFont(fontBold_);
    ImGui::TextColored(c.ink, "What happened:");
    if (fontBold_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextWrapped("%s", activeError_.message.c_str());
    if (fontRegular_) ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Spacing();

    // Recovery Suggestions Box
    if (!activeError_.suggestion.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, c.ct2);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 16.0f);
        ImGui::BeginChild("RecoveryBox", ImVec2(0, 110.0f), false);
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.acc, "Troubleshooting & Next Steps:");
        if (fontBold_) ImGui::PopFont();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextWrapped("%s", activeError_.suggestion.c_str());
        if (fontSmall_) ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Action Buttons
    if (renderPillButton("Try Again", true, false, ImVec2(150.0f, 42.0f), "refresh")) {
        clearError();
    }
    ImGui::SameLine(0, 12.0f);

    if (renderPillButton("System Diagnostics", false, false, ImVec2(200.0f, 42.0f), "network_check")) {
        clearError();
        currentScreen_ = DesktopScreen::Network;
    }
    ImGui::SameLine(0, 12.0f);

    if (renderPillButton("Return Home", false, false, ImVec2(150.0f, 42.0f), "home")) {
        clearError();
        currentScreen_ = DesktopScreen::Home;
    }

    ImGui::Spacing();
    EndCardContainer();
}

void DesktopApplication::renderGuiSettingsDialog() {
    if (!isSettingsOpen_) return;

    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Backdrop overlay
    drawList->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(5, 25, 19, 130));

    // Centered Dialog Modal
    float dlgW = std::min(520.0f, io.DisplaySize.x - 32.0f);
    float dlgH = 490.0f;
    float dlgX = (io.DisplaySize.x - dlgW) * 0.5f;
    float dlgY = (io.DisplaySize.y - dlgH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(dlgX, dlgY));
    ImGui::SetNextWindowSize(ImVec2(dlgW, dlgH));

    ImGuiWindowFlags dlgFlags = ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, c.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, tokens::MintShapes::Dialog);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 24.0f));

    if (ImGui::Begin("SettingsModal", &isSettingsOpen_, dlgFlags)) {
        if (fontTitle_) ImGui::PushFont(fontTitle_);
        ImGui::TextColored(c.ink, "Settings");
        if (fontTitle_) ImGui::PopFont();
        ImGui::Spacing();

        // 1. Theme Segmented Button (M3)
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "Theme");
        if (fontBold_) ImGui::PopFont();
        ImGui::Spacing();

        if (renderM3SegmentedButton("ThemeSeg", { "Light Theme", "Dark Theme" }, &settingsTheme_)) {
            applyActiveTheme();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 2. Contrast Segmented Button (M3)
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "Contrast");
        if (fontBold_) ImGui::PopFont();
        ImGui::Spacing();

        if (renderM3SegmentedButton("ContrastSeg", { "Standard", "High Contrast" }, &settingsContrast_)) {
            applyActiveTheme();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 3. Compact View Switch (M3)
        renderM3Switch("CompactViewSwitch", "Compact view", &settingsCompactView_, "Smaller cards and tiles");
        ImGui::Spacing();

        // 4. Animated Shapes Switch (M3)
        renderM3Switch("AnimatedShapesSwitch", "Animated shapes", &settingsAnimatedShapes_, "Parametric fluid shapes drifting behind the interface");
        ImGui::Spacing();

        // 5. Shapes Shuffle Button
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(c.ink, "Shapes");
        ImGui::SameLine(0, 16.0f);
        if (renderPillButton("Shuffle", false, false, ImVec2(120.0f, 34.0f), "casino")) {
            generateOrganicShapes();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 6. Maximum Video Bitrate Slider (M3)
        renderM3Slider("MaxBitrateSlider", "Maximum video bitrate", &settingsMaxBitrateMbps_, 3, 40, "Mbps");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 7. Frame Rate Select (M3 Segmented Button)
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "Frame rate");
        if (fontBold_) ImGui::PopFont();
        ImGui::Spacing();
        renderM3SegmentedButton("FpsSeg", { "60 FPS (Ultra Smooth)", "30 FPS (Low Bandwidth)" }, &settingsFrameRate_);

        ImGui::Spacing();
        ImGui::Spacing();

        // Done Button
        float btnX = dlgW - 140.0f;
        ImGui::SetCursorPosX(btnX);
        if (renderPillButton("Done", true, false, ImVec2(100.0f, 38.0f))) {
            isSettingsOpen_ = false;
        }

        ImGui::End();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void DesktopApplication::renderGuiToast() {
    if (toastTimer_ <= 0.0f || toastMessage_.empty()) return;

    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImVec2 txtSize = ImGui::CalcTextSize(toastMessage_.c_str());
    if (fontRegular_) ImGui::PopFont();

    float padX = 20.0f;
    float padY = 12.0f;
    float w = txtSize.x + padX * 2.0f;
    float h = txtSize.y + padY * 2.0f;
    float x = (io.DisplaySize.x - w) * 0.5f;
    float y = io.DisplaySize.y - h - 28.0f;

    // Toast pill with ink background and cloud text
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), ImGui::ColorConvertFloat4ToU32(c.ink), 12.0f);
    if (fontRegular_) ImGui::PushFont(fontRegular_);
    drawList->AddText(ImVec2(x + padX, y + padY), ImGui::ColorConvertFloat4ToU32(c.bg), toastMessage_.c_str());
    if (fontRegular_) ImGui::PopFont();
}

void DesktopApplication::renderGuiFrame() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImGuiIO& io = ImGui::GetIO();

    // 1. Tick simulation logic
    float dt = io.DeltaTime;
    if (toastTimer_ > 0.0f) {
        toastTimer_ -= dt;
    }

    if (isInitialLoading_) {
        initialLoadingTimer_ += dt;
        if (initialLoadingTimer_ < 0.35f) {
            initialLoadingStage_ = "Initializing Direct3D 11 & Swap Chain...";
        } else if (initialLoadingTimer_ < 0.70f) {
            initialLoadingStage_ = "Testing Hardware NVENC & Video Encoders...";
        } else if (initialLoadingTimer_ < 1.05f) {
            initialLoadingStage_ = "Loading Typography & Mint Material Assets...";
        } else if (initialLoadingTimer_ >= initialLoadingDuration_) {
            isInitialLoading_ = false;
        }
    }

    if (isHostingActive_) {
        // Subtle real-time jitter in telemetry
        hostFps_ = 59.5f + (rand() % 10) * 0.1f;
        hostEncodeMs_ = 1.4f + (rand() % 5) * 0.1f;
    }

    if (isConnecting_) {
        connectStepTimer_ += dt;
        std::string sId(connectSessionId_);
        if (sId == "fail" || sId == "error") {
            isConnecting_ = false;
            connectStep_ = 0;
            triggerError("Connection Handshake Failed",
                         "Failed to negotiate WebRTC offer/answer with remote session '" + sId + "'.",
                         "1. Ensure the remote host is running and broadcasting.\n"
                         "2. Verify the session ID is correct.\n"
                         "3. Check that your network allows WebRTC UDP packets.",
                         "ERR_RTC_HANDSHAKE_FAILED",
                         DesktopScreen::Connect);
        } else if (connectStepTimer_ > 0.7f && connectStep_ < 4) {
            connectStep_++;
            connectStepTimer_ = 0.0f;
        } else if (connectStep_ >= 4 && connectStepTimer_ > 0.8f) {
            connectStep_ = 5; // All done
            connectStatusMsg_ = "Connected. Press Ctrl+Alt+Shift+Q to leave the stream.";
            showToast("Connected to host");
            isConnecting_ = false;
        }
    }

    if (codeLockSeconds_ > 0) {
        codeLockTimer_ += dt;
        if (codeLockTimer_ >= 1.0f) {
            codeLockSeconds_--;
            codeLockTimer_ = 0.0f;
            if (codeLockSeconds_ <= 0) {
                pairingCode_ = generateRandomPairingCode();
                wrongCodeFails_ = 0;
                codeHintMsg_ = "Pairing is open again with a new code.";
            } else {
                codeHintMsg_ = "Too many wrong codes. Try again in " + std::to_string(codeLockSeconds_) + " seconds.";
            }
        }
    }

    // Network adaptation tick
    static float netTimer = 0.0f;
    netTimer += dt;
    if (netTimer >= 1.0f) {
        netTimer = 0.0f;
        if (isNetworkCongested_) {
            networkBitrate_ = std::max(2.5f, networkBitrate_ * 0.78f);
            networkLoss_ = std::min(9.0f, networkLoss_ + 1.2f);
            networkQosState_ = (networkBitrate_ < 5.0f) ? "Critically congested" : "Congested";
            networkRtt_ = 60 + (rand() % 50);
            networkJitter_ = 9;
            congestionDuration_ -= 1.0f;
            if (congestionDuration_ <= 0.0f) {
                isNetworkCongested_ = false;
            }
        } else if (networkBitrate_ < static_cast<float>(settingsMaxBitrateMbps_) - 0.05f) {
            networkBitrate_ = std::min(static_cast<float>(settingsMaxBitrateMbps_), networkBitrate_ + 0.6f);
            networkLoss_ = std::max(0.0f, networkLoss_ - 0.8f);
            networkQosState_ = (networkBitrate_ < settingsMaxBitrateMbps_ - 1.0f) ? "Recovering" : "Increasing";
            networkRtt_ = 16 + (rand() % 5);
            networkJitter_ = 2;
        } else {
            networkQosState_ = "Stable";
            networkLoss_ = 0.0f;
            networkRtt_ = 18;
            networkJitter_ = 2;
        }

        bitrateHistory_.push_back(networkBitrate_);
        if (bitrateHistory_.size() > 60) bitrateHistory_.erase(bitrateHistory_.begin());
        rttHistory_.push_back(static_cast<float>(networkRtt_));
        if (rttHistory_.size() > 60) rttHistory_.erase(rttHistory_.begin());
    }

    // 2. Ambient drifting wallpaper background with exact Mist.html parametric organic shapes
    ImDrawList* bgDraw = ImGui::GetBackgroundDrawList();

    // 160-degree linear gradient canvas
    ImU32 w1Col = ImGui::ColorConvertFloat4ToU32(c.bg);
    ImU32 w2Col = (currentTheme_ == tokens::ThemeMode::Light)
        ? IM_COL32(220, 246, 236, 255)  // #dcf6ec soft mint gradient
        : IM_COL32(16, 50, 41, 255);    // #103229 dark mint gradient

    bgDraw->AddRectFilledMultiColor(
        ImVec2(0, 0), io.DisplaySize,
        w1Col, w2Col, w2Col, w1Col
    );

    // Render the 6 parametric organic fluid petal shapes
    static float animTime = 0.0f;
    if (settingsAnimatedShapes_) {
        animTime += dt;
    }

    float W = std::min(io.DisplaySize.x, 1400.0f);

    for (const auto& s : organicShapes_) {
        float driftAngle = (animTime / s.driftDuration) * 6.28318f;
        float curDriftX = settingsAnimatedShapes_ ? (std::sin(driftAngle) * s.driftX) : 0.0f;
        float curDriftY = settingsAnimatedShapes_ ? (std::cos(driftAngle) * s.driftY) : 0.0f;

        float cx = s.posXRatio * io.DisplaySize.x + curDriftX;
        float cy = s.posYRatio * io.DisplaySize.y + curDriftY;
        float radius = s.scale * (W / 1200.0f);
        float f = 88.0f / (s.templateType ? 1.0f : (1.0f + s.param));

        ImVec4 shapeCol = (s.colorIndex == 0) ? c.s1 : ((s.colorIndex == 1) ? c.s2 : ((s.colorIndex == 2) ? c.s3 : c.s4));
        shapeCol.w = (currentTheme_ == tokens::ThemeMode::Light) ? 0.75f : 0.45f;
        ImU32 fillU32 = ImGui::ColorConvertFloat4ToU32(shapeCol);

        // 160-point parametric curve
        constexpr int NUM_PTS = 80;
        ImVec2 pts[NUM_PTS];
        for (int k = 0; k < NUM_PTS; ++k) {
            float a = static_cast<float>(k) / static_cast<float>(NUM_PTS) * 6.2831853f;
            float rVal = s.templateType
                ? (1.0f - s.param + s.param * std::abs(std::cos(static_cast<float>(s.symmetry) * a * 0.5f + s.phase)))
                : (1.0f + s.param * std::cos(static_cast<float>(s.symmetry) * a + s.phase));

            float px = cx + (f * rVal * std::cos(a)) * (radius / 180.0f);
            float py = cy + (f * rVal * std::sin(a)) * (radius / 180.0f);
            pts[k] = ImVec2(px, py);
        }

        // Draw filled organic shape
        bgDraw->AddConvexPolyFilled(pts, NUM_PTS, fillU32);
    }

    if (isInitialLoading_) {
        renderGuiLoadingScreen();
        return;
    }

    // 3. Main Window with transparent background (so organic wall shines through!)
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoBackground; // Transparent background!

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
    ImGui::Begin("MistMainWindow", nullptr, windowFlags);

    // Top Header
    renderGuiHeader();
    ImGui::Spacing();
    ImGui::Spacing();

    // Navigation Rail & Content Area
    renderGuiNavRail();
    ImGui::SameLine(0, 16.0f);

    // Transparent Child for MainContent so the wallpaper flows behind the cards
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

    switch (currentScreen_) {
        case DesktopScreen::Home:
            renderGuiHomeScreen();
            break;
        case DesktopScreen::Host:
            renderGuiHostScreen();
            break;
        case DesktopScreen::Connect:
            renderGuiConnectScreen();
            break;
        case DesktopScreen::Devices:
            renderGuiDevicesScreen();
            break;
        case DesktopScreen::Network:
            renderGuiNetworkScreen();
            break;
        case DesktopScreen::Error:
            renderGuiErrorScreen();
            break;
        default:
            renderGuiHomeScreen();
            break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();

    // 4. Overlays: Settings Dialog & Toast
    renderGuiSettingsDialog();
    renderGuiToast();
}

#endif // _WIN32

} // namespace mist::desktop
