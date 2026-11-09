#include "DesktopApplication.h"
#include "mist/common/Logger.h"
#include "mist/platform/windows/PlatformWindows.h"

#if defined(_WIN32)
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <shellscalingapi.h>
#include <wincodec.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#include <xinput.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
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
        case WM_KEYDOWN:
            if (wParam == VK_F11) {
                auto* app = reinterpret_cast<mist::desktop::DesktopApplication*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
                if (app) {
                    app->toggleFullscreen();
                }
                return 0;
            }
            break;
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

static uint32_t getMaterialSymbolCodepoint(const std::string& name) {
    if (name == "menu") return 0xe5d2;
    if (name == "search") return 0xe8b6;
    if (name == "home") return 0xe88a;
    if (name == "cast") return 0xe307;
    if (name == "sports_esports") return 0xea28;
    if (name == "videogame_asset" || name == "pad" || name == "gamepad") return 0xe30f;
    if (name == "devices") return 0xe337;
    if (name == "settings") return 0xe8b8;
    if (name == "network_check" || name == "speed") return 0xe1c4;
    if (name == "network" || name == "wifi") return 0xe63e;
    if (name == "check_circle") return 0xe86c;
    if (name == "radio_button_unchecked" || name == "circle") return 0xe836;
    if (name == "error") return 0xe000;
    if (name == "warning") return 0xe002;
    if (name == "refresh") return 0xe5d5;
    if (name == "bolt") return 0xea0b;
    if (name == "close") return 0xe5cd;
    if (name == "play_arrow") return 0xe037;
    if (name == "stop") return 0xe047;
    if (name == "casino") return 0xeb40;
    if (name == "router" || name == "dns") return 0xe875;
    if (name == "tune") return 0xe429;
    if (name == "link") return 0xe157;
    if (name == "desktop_windows" || name == "desktop" || name == "monitor") return 0xe30c;
    if (name == "phonelink") return 0xe326;
    if (name == "palette" || name == "dark_mode") return 0xe40a;
    if (name == "light_mode") return 0xe518;
    if (name == "sensors") return 0xe51e;
    if (name == "keyboard") return 0xe312;
    if (name == "mouse") return 0xe315;
    if (name == "lock" || name == "security") return 0xe897;
    if (name == "arrow_forward") return 0xe5c8;
    return 0;
}

static std::string codepointToUtf8(uint32_t cp) {
    std::string s;
    if (cp == 0) return s;
    if (cp <= 0x7F) {
        s.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        s.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        s.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        s.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return s;
}

static void BeginCardContainer(const char* id, const mist::desktop::tokens::ThemeColors& c, const ImVec2& size = ImVec2(0, 0), float rounding = 28.0f) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, c.surface);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 22.0f));

    ImGuiChildFlags childFlags = ImGuiChildFlags_Borders;
    if (size.y <= 0.0f) {
        childFlags |= ImGuiChildFlags_AutoResizeY;
    }
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild(id, size, childFlags, windowFlags);
}

static void EndCardContainer() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

static std::wstring findExecutablePath(const std::wstring& exeName) {
    wchar_t currentExe[MAX_PATH];
    if (GetModuleFileNameW(nullptr, currentExe, MAX_PATH)) {
        std::wstring dir(currentExe);
        size_t lastSlash = dir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            std::wstring baseDir = dir.substr(0, lastSlash);
            std::wstring candidates[] = {
                baseDir + L"\\" + exeName,
                baseDir + L"\\..\\host\\" + exeName,
                baseDir + L"\\..\\client\\" + exeName,
                baseDir + L"\\..\\..\\apps\\host\\" + exeName,
                baseDir + L"\\..\\..\\apps\\client\\" + exeName,
                baseDir + L"\\..\\..\\..\\build\\apps\\host\\" + exeName,
                baseDir + L"\\..\\..\\..\\build\\apps\\client\\" + exeName,
                L"build\\apps\\host\\" + exeName,
                L"build\\apps\\client\\" + exeName,
                L"C:\\Users\\mumer\\OneDrive\\Desktop\\mist\\build\\apps\\host\\" + exeName,
                L"C:\\Users\\mumer\\OneDrive\\Desktop\\mist\\build\\apps\\client\\" + exeName,
                L"C:\\Users\\mumer\\AppData\\Local\\Programs\\Mist\\" + exeName,
                L"C:\\Users\\mumer\\AppData\\Local\\Programs\\Mist\\bin\\" + exeName
            };
            for (const auto& cPath : candidates) {
                if (GetFileAttributesW(cPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    return cPath;
                }
            }
        }
    }
    return exeName;
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
#if defined(_WIN32)
    stopNetworkMonitor();
    terminateHostProcess();
    terminateClientProcess();
#endif
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

void DesktopApplication::toggleFullscreen() {
    if (!hwnd_) return;

    DWORD dwStyle = GetWindowLongW(hwnd_, GWL_STYLE);
    if (!isFullscreenWindow_) {
        prevWindowPlacement_.length = sizeof(WINDOWPLACEMENT);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetWindowPlacement(hwnd_, &prevWindowPlacement_) &&
            GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongW(hwnd_, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd_, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            isFullscreenWindow_ = true;
            isStreamingFullscreen_ = true;
            showToast("Entered Fullscreen mode (F11)");
        }
    } else {
        SetWindowLongW(hwnd_, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd_, &prevWindowPlacement_);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        isFullscreenWindow_ = false;
        isStreamingFullscreen_ = false;
        showToast("Exited Fullscreen mode (F11)");
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

void DesktopApplication::startNetworkMonitor() {
    if (networkThreadRunning_.load()) return;
    networkThreadRunning_.store(true);
    networkThread_ = std::thread(&DesktopApplication::networkMonitorWorker, this);
}

void DesktopApplication::stopNetworkMonitor() {
    if (!networkThreadRunning_.load()) return;
    networkThreadRunning_.store(false);
    if (networkThread_.joinable()) {
        networkThread_.join();
    }
}

void DesktopApplication::networkMonitorWorker() {
    probeLiveHardware();
    probeLiveNetwork();

    int hwCounter = 0;
    while (networkThreadRunning_.load()) {
        probeLiveNetwork();

        if (++hwCounter >= 3) {
            hwCounter = 0;
            probeLiveHardware();
        }

        for (int i = 0; i < 20 && networkThreadRunning_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void DesktopApplication::probeLiveHardware() {
    std::lock_guard<std::mutex> lock(networkMutex_);

    // 1. DXGI GPU Check
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 d{};
            if (SUCCEEDED(adapter->GetDesc1(&d))) {
                if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                    char nameBuf[256]{};
                    WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, nameBuf, sizeof(nameBuf) - 1, nullptr, nullptr);
                    liveSystemHardware_.gpuName = nameBuf;
                    liveSystemHardware_.hasNvenc = (d.VendorId == 0x10DE); // NVIDIA NVENC
                    break;
                }
            }
        }
    }

    // 2. Display Resolution & Refresh Rate
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    int refreshRate = 60;
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 0) {
        refreshRate = static_cast<int>(dm.dmDisplayFrequency);
    }
    liveSystemHardware_.displayInfo = std::to_string(screenW) + "x" + std::to_string(screenH) + " @ " + std::to_string(refreshRate) + "Hz";

    // 3. Audio output endpoint
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
        Microsoft::WRL::ComPtr<IMMDevice> defaultDevice;
        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
            Microsoft::WRL::ComPtr<IPropertyStore> props;
            if (SUCCEEDED(defaultDevice->OpenPropertyStore(STGM_READ, &props))) {
                PROPVARIANT varName;
                PropVariantInit(&varName);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.vt == VT_LPWSTR && varName.pwszVal) {
                    char audioBuf[256]{};
                    WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, audioBuf, sizeof(audioBuf) - 1, nullptr, nullptr);
                    liveSystemHardware_.audioEndpoint = audioBuf;
                }
                PropVariantClear(&varName);
            }
        }
    }

    // 4. Controller Check via XInput
    HMODULE hXInput = LoadLibraryW(L"xinput1_4.dll");
    if (!hXInput) hXInput = LoadLibraryW(L"xinput9_1_0.dll");
    if (hXInput) {
        using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        auto pGetState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(hXInput, "XInputGetState"));
        if (pGetState) {
            bool foundPad = false;
            for (DWORD i = 0; i < 4; ++i) {
                XINPUT_STATE state{};
                if (pGetState(i, &state) == ERROR_SUCCESS) {
                    liveSystemHardware_.hasGamepad = true;
                    liveSystemHardware_.gamepadStatus = "Controller " + std::to_string(i + 1) + " connected";
                    foundPad = true;
                    break;
                }
            }
            if (!foundPad) {
                liveSystemHardware_.hasGamepad = false;
                liveSystemHardware_.gamepadStatus = "No controller detected";
            }
        }
        FreeLibrary(hXInput);
    }
}

void DesktopApplication::probeLiveNetwork() {
    LiveNetworkStatus statusCopy;
    std::vector<NearbyWifiNetwork> scannedAps;

    // 1. Native Wi-Fi Query via WlanAPI
    HANDLE hWlan = nullptr;
    DWORD dwVersion = 0;
    DWORD dwResult = WlanOpenHandle(2, nullptr, &dwVersion, &hWlan);
    if (dwResult == ERROR_SUCCESS && hWlan) {
        PWLAN_INTERFACE_INFO_LIST pIntfList = nullptr;
        if (WlanEnumInterfaces(hWlan, nullptr, &pIntfList) == ERROR_SUCCESS && pIntfList) {
            for (DWORD i = 0; i < pIntfList->dwNumberOfItems; ++i) {
                WLAN_INTERFACE_INFO* pInfo = &pIntfList->InterfaceInfo[i];
                if (pInfo->isState == wlan_interface_state_connected) {
                    statusCopy.isWifi = true;

                    DWORD dataSize = 0;
                    PWLAN_CONNECTION_ATTRIBUTES pAttr = nullptr;
                    if (WlanQueryInterface(hWlan, &pInfo->InterfaceGuid, wlan_intf_opcode_current_connection, nullptr, &dataSize, (PVOID*)&pAttr, nullptr) == ERROR_SUCCESS && pAttr) {
                        DOT11_SSID& dot11Ssid = pAttr->wlanAssociationAttributes.dot11Ssid;
                        if (dot11Ssid.uSSIDLength > 0 && dot11Ssid.uSSIDLength <= 32) {
                            statusCopy.wifiSsid = std::string(reinterpret_cast<char*>(dot11Ssid.ucSSID), dot11Ssid.uSSIDLength);
                        }
                        statusCopy.wifiSignalQualityPercent = static_cast<int>(pAttr->wlanAssociationAttributes.wlanSignalQuality);
                        statusCopy.wifiRssiDbm = -100 + (statusCopy.wifiSignalQualityPercent / 2);

                        switch (pAttr->wlanAssociationAttributes.dot11PhyType) {
                            case dot11_phy_type_he:  statusCopy.wifiStandard = "Wi-Fi 6 (802.11ax)"; break;
                            case dot11_phy_type_vht: statusCopy.wifiStandard = "Wi-Fi 5 (802.11ac)"; break;
                            case dot11_phy_type_ht:  statusCopy.wifiStandard = "Wi-Fi 4 (802.11n)"; break;
                            case dot11_phy_type_erp: statusCopy.wifiStandard = "Wi-Fi 3 (802.11g)"; break;
                            default:                 statusCopy.wifiStandard = "Wi-Fi (802.11)"; break;
                        }

                        uint64_t rxKbps = pAttr->wlanAssociationAttributes.ulRxRate;
                        uint64_t txKbps = pAttr->wlanAssociationAttributes.ulTxRate;
                        statusCopy.linkSpeedMbps = std::max<uint64_t>(10, std::max(rxKbps, txKbps) / 1000);

                        WlanFreeMemory(pAttr);
                    }

                    // Query BSS list for channel frequency
                    PWLAN_BSS_LIST pBssList = nullptr;
                    if (WlanGetNetworkBssList(hWlan, &pInfo->InterfaceGuid, nullptr, dot11_BSS_type_any, FALSE, nullptr, &pBssList) == ERROR_SUCCESS && pBssList) {
                        for (DWORD j = 0; j < pBssList->dwNumberOfItems; ++j) {
                            WLAN_BSS_ENTRY& bss = pBssList->wlanBssEntries[j];
                            std::string bssSsid(reinterpret_cast<char*>(bss.dot11Ssid.ucSSID), bss.dot11Ssid.uSSIDLength);
                            if (bssSsid == statusCopy.wifiSsid) {
                                statusCopy.wifiRssiDbm = bss.lRssi;
                                statusCopy.wifiFrequencyGhz = static_cast<float>(bss.ulChCenterFrequency) / 1000000.0f;
                                if (bss.ulChCenterFrequency >= 2412000 && bss.ulChCenterFrequency <= 2484000) {
                                    statusCopy.wifiChannel = static_cast<int>((bss.ulChCenterFrequency - 2407000) / 5000);
                                } else if (bss.ulChCenterFrequency >= 5000000 && bss.ulChCenterFrequency <= 5900000) {
                                    statusCopy.wifiChannel = static_cast<int>((bss.ulChCenterFrequency - 5000000) / 5000);
                                }
                                break;
                            }
                        }
                        WlanFreeMemory(pBssList);
                    }
                    break;
                }
            }

            // Scan nearby available networks
            if (pIntfList->dwNumberOfItems > 0) {
                PWLAN_AVAILABLE_NETWORK_LIST pAvailList = nullptr;
                if (WlanGetAvailableNetworkList(hWlan, &pIntfList->InterfaceInfo[0].InterfaceGuid, 0, nullptr, &pAvailList) == ERROR_SUCCESS && pAvailList) {
                    for (DWORD k = 0; k < pAvailList->dwNumberOfItems && scannedAps.size() < 6; ++k) {
                        WLAN_AVAILABLE_NETWORK& net = pAvailList->Network[k];
                        if (net.dot11Ssid.uSSIDLength > 0) {
                            NearbyWifiNetwork entry;
                            entry.ssid = std::string(reinterpret_cast<char*>(net.dot11Ssid.ucSSID), net.dot11Ssid.uSSIDLength);
                            entry.signalPercent = static_cast<int>(net.wlanSignalQuality);
                            entry.rssiDbm = -100 + (entry.signalPercent / 2);
                            entry.security = (net.dot11DefaultAuthAlgorithm == DOT11_AUTH_ALGO_RSNA_PSK) ? "WPA2/WPA3" : "Secured";
                            scannedAps.push_back(entry);
                        }
                    }
                    WlanFreeMemory(pAvailList);
                }
            }

            WlanFreeMemory(pIntfList);
        }
        WlanCloseHandle(hWlan, nullptr);
    }

    // 2. IP Helper Query (GetAdaptersAddresses)
    ULONG outBufLen = 16384;
    std::vector<uint8_t> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, pAddresses, &outBufLen);
    }

    DWORD activeIfIndex = 0;
    if (dwRetVal == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pBestAdapter = nullptr;
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr; pCurr = pCurr->Next) {
            if (pCurr->OperStatus == IfOperStatusUp && pCurr->IfType != IF_TYPE_SOFTWARE_LOOPBACK && pCurr->FirstGatewayAddress) {
                std::wstring desc = pCurr->Description ? pCurr->Description : L"";
                bool isVirtual = (desc.find(L"Virtual") != std::wstring::npos ||
                                  desc.find(L"Hyper-V") != std::wstring::npos ||
                                  desc.find(L"WSL") != std::wstring::npos ||
                                  desc.find(L"TAP") != std::wstring::npos);
                if (!isVirtual || !pBestAdapter) {
                    pBestAdapter = pCurr;
                    if (!isVirtual) break;
                }
            }
        }

        if (pBestAdapter) {
            activeIfIndex = pBestAdapter->IfIndex;
            char friendlyBuf[256]{};
            if (pBestAdapter->FriendlyName) {
                WideCharToMultiByte(CP_UTF8, 0, pBestAdapter->FriendlyName, -1, friendlyBuf, sizeof(friendlyBuf) - 1, nullptr, nullptr);
            }
            char descBuf[256]{};
            if (pBestAdapter->Description) {
                WideCharToMultiByte(CP_UTF8, 0, pBestAdapter->Description, -1, descBuf, sizeof(descBuf) - 1, nullptr, nullptr);
            }
            statusCopy.adapterName = (descBuf[0] != '\0') ? descBuf : friendlyBuf;

            if (pBestAdapter->TransmitLinkSpeed > 0) {
                statusCopy.linkSpeedMbps = pBestAdapter->TransmitLinkSpeed / 1000000ULL;
            }

            if (pBestAdapter->IfType == IF_TYPE_IEEE80211) {
                statusCopy.isWifi = true;
                statusCopy.connectionType = statusCopy.wifiStandard.empty() ? "Wi-Fi (802.11)" : statusCopy.wifiStandard;
            } else {
                statusCopy.isWifi = false;
                statusCopy.connectionType = "Wired Ethernet (802.3)";
            }

            // IP Address
            if (pBestAdapter->FirstUnicastAddress && pBestAdapter->FirstUnicastAddress->Address.lpSockaddr) {
                sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(pBestAdapter->FirstUnicastAddress->Address.lpSockaddr);
                char ipStr[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &(sa_in->sin_addr), ipStr, INET_ADDRSTRLEN);
                statusCopy.ipv4Address = ipStr;
            }

            // Gateway Address
            if (pBestAdapter->FirstGatewayAddress && pBestAdapter->FirstGatewayAddress->Address.lpSockaddr) {
                sockaddr_in* sa_gw = reinterpret_cast<sockaddr_in*>(pBestAdapter->FirstGatewayAddress->Address.lpSockaddr);
                char gwStr[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &(sa_gw->sin_addr), gwStr, INET_ADDRSTRLEN);
                statusCopy.gatewayAddress = gwStr;
            }
        }
    }

    // 3. Query Throughput via GetIfEntry2
    if (activeIfIndex > 0) {
        MIB_IF_ROW2 ifRow{};
        ifRow.InterfaceIndex = activeIfIndex;
        if (GetIfEntry2(&ifRow) == NO_ERROR) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - prevSampleTime_).count();
            if (dt >= 0.5f && prevInOctets_ > 0) {
                uint64_t inDelta = (ifRow.InOctets >= prevInOctets_) ? (ifRow.InOctets - prevInOctets_) : 0;
                uint64_t outDelta = (ifRow.OutOctets >= prevOutOctets_) ? (ifRow.OutOctets - prevOutOctets_) : 0;
                statusCopy.rxThroughputMbps = (static_cast<float>(inDelta) * 8.0f) / (dt * 1000000.0f);
                statusCopy.txThroughputMbps = (static_cast<float>(outDelta) * 8.0f) / (dt * 1000000.0f);
            }
            prevInOctets_ = ifRow.InOctets;
            prevOutOctets_ = ifRow.OutOctets;
            prevSampleTime_ = now;
        }
    }

    // 4. Live ICMP Echo Ping (Gateway & 8.8.8.8)
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp != INVALID_HANDLE_VALUE) {
        if (!statusCopy.gatewayAddress.empty()) {
            IPAddr gwAddr = INADDR_NONE;
            inet_pton(AF_INET, statusCopy.gatewayAddress.c_str(), &gwAddr);
            if (gwAddr != INADDR_NONE) {
                char sendData[32] = "Mist Echo";
                char replyBuffer[sizeof(ICMP_ECHO_REPLY) + 32 + 8];
                DWORD dwReplies = IcmpSendEcho(hIcmp, gwAddr, sendData, sizeof(sendData), nullptr, replyBuffer, sizeof(replyBuffer), 70);
                if (dwReplies > 0) {
                    PICMP_ECHO_REPLY pEchoReply = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer);
                    statusCopy.gatewayPingMs = static_cast<int>(pEchoReply->RoundTripTime);
                    if (prevGatewayPing_ >= 0) {
                        int delta = std::abs(statusCopy.gatewayPingMs - prevGatewayPing_);
                        statusCopy.jitterMs = (liveNetworkStatus_.jitterMs * 3 + delta) / 4;
                    }
                    prevGatewayPing_ = statusCopy.gatewayPingMs;
                }
            }
        }

        // WAN Ping to 8.8.8.8
        IPAddr dnsAddr = INADDR_NONE;
        inet_pton(AF_INET, "8.8.8.8", &dnsAddr);
        if (dnsAddr != INADDR_NONE) {
            char sendData[32] = "Mist WAN";
            char replyBuffer[sizeof(ICMP_ECHO_REPLY) + 32 + 8];
            DWORD dwReplies = IcmpSendEcho(hIcmp, dnsAddr, sendData, sizeof(sendData), nullptr, replyBuffer, sizeof(replyBuffer), 120);
            if (dwReplies > 0) {
                PICMP_ECHO_REPLY pEchoReply = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer);
                statusCopy.internetPingMs = static_cast<int>(pEchoReply->RoundTripTime);
            }
        }
        IcmpCloseHandle(hIcmp);
    }

    // 5. QoS State Evaluation
    if (statusCopy.gatewayPingMs <= 2 && statusCopy.jitterMs <= 2) {
        statusCopy.qosState = "Optimal (Zero Jitter)";
    } else if (statusCopy.gatewayPingMs <= 15 && statusCopy.jitterMs <= 5) {
        statusCopy.qosState = "Stable";
    } else if (statusCopy.gatewayPingMs <= 35) {
        statusCopy.qosState = "Acceptable";
    } else {
        statusCopy.qosState = "High Latency";
    }

    statusCopy.nearbyWifiList = scannedAps;

    // 6. Safely store back
    {
        std::lock_guard<std::mutex> lock(networkMutex_);
        liveNetworkStatus_ = statusCopy;
    }
}

bool DesktopApplication::launchHostProcess(const std::string& sessionId, const std::string& signalingUrl, bool enableAudio, bool allowInput) {
    terminateHostProcess();

    std::wstring exePath = findExecutablePath(L"mist_host.exe");
    std::wstring wSessionId(sessionId.begin(), sessionId.end());
    std::wstring wSignaling(signalingUrl.begin(), signalingUrl.end());

    std::wstring cmdLine = L"\"" + exePath + L"\" --stream-live --session-id \"" + wSessionId + L"\" --signaling-url \"" + wSignaling + L"\"";
    if (enableAudio) {
        cmdLine += L" --audio";
    }
    if (allowInput) {
        cmdLine += L" --allow-remote-input";
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        DWORD err = GetLastError();
        triggerError("Failed to Launch Streaming Host",
                     "Could not spawn mist_host.exe process. Error code: " + std::to_string(err),
                     "Verify that mist_host.exe exists in the bin directory and is executable.",
                     "ERR_HOST_SPAWN_FAILED",
                     DesktopScreen::Host);
        return false;
    }

    hHostProcess_ = pi.hProcess;
    hostProcessPid_ = pi.dwProcessId;
    CloseHandle(pi.hThread);
    isHostingActive_ = true;
    return true;
}

void DesktopApplication::terminateHostProcess() {
    if (hHostProcess_) {
        TerminateProcess(hHostProcess_, 0);
        WaitForSingleObject(hHostProcess_, 500);
        CloseHandle(hHostProcess_);
        hHostProcess_ = nullptr;
        hostProcessPid_ = 0;
    }
    isHostingActive_ = false;
}

bool DesktopApplication::launchClientProcess(const std::string& sessionId, const std::string& signalingUrl, bool hearAudio, bool sendInput) {
    terminateClientProcess();

    std::wstring exePath = findExecutablePath(L"mist_client.exe");
    std::wstring wSessionId(sessionId.begin(), sessionId.end());
    std::wstring wSignaling(signalingUrl.begin(), signalingUrl.end());

    std::wstring cmdLine = L"\"" + exePath + L"\" --stream-receive --session-id \"" + wSessionId + L"\" --signaling-url \"" + wSignaling + L"\"";
    if (hearAudio) {
        cmdLine += L" --audio";
    }
    if (sendInput) {
        cmdLine += L" --enable-input";
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!ok) {
        DWORD err = GetLastError();
        triggerError("Failed to Launch Client Session",
                     "Could not spawn mist_client.exe process. Error code: " + std::to_string(err),
                     "Verify that mist_client.exe exists in the bin directory and is executable.",
                     "ERR_CLIENT_SPAWN_FAILED",
                     DesktopScreen::Connect);
        return false;
    }

    hClientProcess_ = pi.hProcess;
    clientProcessPid_ = pi.dwProcessId;
    CloseHandle(pi.hThread);
    isConnecting_ = true;
    connectStep_ = 4;
    connectStatusMsg_ = "Streaming session connected (PID " + std::to_string(clientProcessPid_) + ").";
    return true;
}

void DesktopApplication::terminateClientProcess() {
    if (hClientProcess_) {
        TerminateProcess(hClientProcess_, 0);
        WaitForSingleObject(hClientProcess_, 500);
        CloseHandle(hClientProcess_);
        hClientProcess_ = nullptr;
        clientProcessPid_ = 0;
    }
    isConnecting_ = false;
    connectStep_ = 0;
}

void DesktopApplication::checkBackgroundProcesses() {
    if (hHostProcess_) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(hHostProcess_, &exitCode) && exitCode != STILL_ACTIVE) {
            CloseHandle(hHostProcess_);
            hHostProcess_ = nullptr;
            hostProcessPid_ = 0;
            isHostingActive_ = false;
            showToast("Host stream ended (code " + std::to_string(exitCode) + ")");
        }
    }

    if (hClientProcess_) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(hClientProcess_, &exitCode) && exitCode != STILL_ACTIVE) {
            CloseHandle(hClientProcess_);
            hClientProcess_ = nullptr;
            clientProcessPid_ = 0;
            isConnecting_ = false;
            connectStep_ = 0;
            connectStatusMsg_ = "Client stream session closed.";
            showToast("Client stream session closed.");
        }
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

    // Load Material Symbols Rounded dedicated fonts (only required symbol ranges)
    if (!fontMatIcons.empty()) {
        static const ImWchar icon_ranges[] = {
            0xe000, 0xe050,  // error, warning, play_arrow, stop
            0xe150, 0xe1d0,  // link, network_check, speed
            0xe300, 0xe340,  // cast, videogame_asset, devices, phonelink, keyboard, mouse, desktop
            0xe400, 0xe435,  // palette, tune
            0xe510, 0xe5e0,  // light_mode, sensors, menu, refresh, close
            0xe630, 0xe645,  // wifi
            0xe830, 0xe8e0,  // circle, check_circle, router, home, search, settings
            0xea00, 0xea30,  // bolt, sports_esports
            0xeb40, 0xeb45,  // casino
            0
        };
        ImFontConfig iconCfg;
        iconCfg.PixelSnapH = true;
        iconCfg.GlyphOffset.y = 1.0f;
        fontIcons_ = io.Fonts->AddFontFromFileTTF(fontMatIcons.c_str(), 24.0f, &iconCfg, icon_ranges);
        fontIconsLarge_ = io.Fonts->AddFontFromFileTTF(fontMatIcons.c_str(), 40.0f, &iconCfg, icon_ranges);
    }

    if (!fontRegular_) {
        fontRegular_ = io.Fonts->AddFontDefault();
    }

    generateOrganicShapes();
    applyActiveTheme();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(d3d11Device_.Get(), d3d11Context_.Get());

    // Pre-clear backbuffer so window never displays unrendered black buffer on launch
    const auto& c = tokens::getActiveTheme(currentTheme_);
    const float clearColor[4] = { c.bg.x, c.bg.y, c.bg.z, 1.00f };
    if (rtv_ && d3d11Context_) {
        ID3D11RenderTargetView* rtvPtr = rtv_.Get();
        d3d11Context_->OMSetRenderTargets(1, &rtvPtr, nullptr);
        d3d11Context_->ClearRenderTargetView(rtvPtr, clearColor);
        if (swapChain_) {
            (void)swapChain_->Present(0, 0);
        }
    }

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
    fontIcons_ = nullptr;
    fontIconsLarge_ = nullptr;

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
    startNetworkMonitor();

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

    stopNetworkMonitor();
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
    std::string icon = name ? name : "";
    uint32_t cp = getMaterialSymbolCodepoint(icon);
    std::string utf8Icon = codepointToUtf8(cp);

    if (!utf8Icon.empty() && (fontIcons_ || fontIconsLarge_)) {
        ImFont* font = (size >= 28.0f && fontIconsLarge_) ? fontIconsLarge_ : (fontIcons_ ? fontIcons_ : fontIconsLarge_);
        if (font) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 glyphSize = font->CalcTextSizeA(size, FLT_MAX, -1.0f, utf8Icon.c_str());
            ImVec2 textPos(center.x - glyphSize.x * 0.5f, center.y - glyphSize.y * 0.5f);
            drawList->AddText(font, size, textPos, color, utf8Icon.c_str());
            return;
        }
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float cx = center.x;
    float cy = center.y;

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

void DesktopApplication::navigateToScreen(DesktopScreen screen) {
    if (currentScreen_ != screen) {
        clearError();
        transitionTarget_ = screen;
        currentScreen_ = screen;
        screenAlpha_ = 0.0f;
        screenSlide_ = 14.0f;
    }
}

float DesktopApplication::getAnim(const std::string& key, float target, float speed) {
    auto it = animMap_.find(key);
    if (it == animMap_.end()) {
        animMap_[key] = target;
        return target;
    }
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    float current = it->second;
    current += (target - current) * std::min(1.0f, dt * speed);
    it->second = current;
    return current;
}

bool DesktopApplication::renderPillButton(const char* label, bool primary, bool danger, const ImVec2& size, const char* iconName) {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImVec2 actualSize = size;
    if (actualSize.y <= 0) actualSize.y = 44.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens::MintShapes::Pill);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24.0f, 10.0f));

    ImU32 iconCol = 0;
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, c.mint);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.mint.x * 1.08f, c.mint.y * 1.08f, c.mint.z * 1.08f, 1.0f));
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
    std::string fullLabel = (iconName ? "     " : "") + std::string(label);
    if (fontBold_) ImGui::PushFont(fontBold_);
    bool clicked = ImGui::Button(fullLabel.c_str(), actualSize);
    if (fontBold_) ImGui::PopFont();

    if (iconName) {
        renderVectorIcon(iconName, ImVec2(pStart.x + 22.0f, pStart.y + actualSize.y * 0.5f), 20.0f, iconCol);
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

    // Animated toggle ratio: 0.0 = off, 1.0 = on
    float t = getAnim(std::string("switch_") + id, *v ? 1.0f : 0.0f, 18.0f);

    // Draw M3 Switch track on right
    float trackX = pStart.x + rowW - trackW - 4.0f;
    float trackY = pStart.y + (rowH - trackH) * 0.5f;

    ImVec2 trackMin(trackX, trackY);
    ImVec2 trackMax(trackX + trackW, trackY + trackH);

    // Track color interpolation
    ImVec4 offTrackCol = c.ct2;
    ImVec4 onTrackCol = c.mint;
    ImVec4 curTrackCol = ImVec4(
        offTrackCol.x + (onTrackCol.x - offTrackCol.x) * t,
        offTrackCol.y + (onTrackCol.y - offTrackCol.y) * t,
        offTrackCol.z + (onTrackCol.z - offTrackCol.z) * t,
        1.0f
    );
    drawList->AddRectFilled(trackMin, trackMax, ImGui::ColorConvertFloat4ToU32(curTrackCol), trackRadius);

    if (t < 0.95f) {
        ImVec4 borderCol = isHovered ? c.ink : c.mute;
        borderCol.w = (1.0f - t);
        drawList->AddRect(trackMin, trackMax, ImGui::ColorConvertFloat4ToU32(borderCol), trackRadius, 0, 1.8f);
    }

    // Thumb position & size interpolation
    float thumbMinX = trackMin.x + 16.0f;
    float thumbMaxX = trackMax.x - 16.0f;
    float curThumbX = thumbMinX + (thumbMaxX - thumbMinX) * t;
    float curThumbRadius = 8.0f + 4.0f * t;

    ImVec2 thumbCenter(curThumbX, trackMin.y + 16.0f);

    // Thumb color interpolation
    ImVec4 offThumbCol = isHovered ? c.ink : c.mute;
    ImVec4 onThumbCol = c.on;
    ImVec4 curThumbCol = ImVec4(
        offThumbCol.x + (onThumbCol.x - offThumbCol.x) * t,
        offThumbCol.y + (onThumbCol.y - offThumbCol.y) * t,
        offThumbCol.z + (onThumbCol.z - offThumbCol.z) * t,
        1.0f
    );
    drawList->AddCircleFilled(thumbCenter, curThumbRadius, ImGui::ColorConvertFloat4ToU32(curThumbCol));

    // Checkmark inside active thumb
    if (t > 0.4f) {
        float alphaCheck = (t - 0.4f) / 0.6f;
        ImVec4 checkCol = c.acc;
        checkCol.w = alphaCheck;
        ImVec2 pChk[3] = {
            ImVec2(thumbCenter.x - 4.0f, thumbCenter.y),
            ImVec2(thumbCenter.x - 1.0f, thumbCenter.y + 3.0f),
            ImVec2(thumbCenter.x + 4.0f, thumbCenter.y - 3.0f)
        };
        drawList->AddPolyline(pChk, 3, ImGui::ColorConvertFloat4ToU32(checkCol), ImDrawFlags_None, 1.8f);
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

    // 0. Circular Menu Button (Collapses/Expands Navigation Rail)
    ImVec2 menuPos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens::MintShapes::Pill);
    ImGui::PushStyleColor(ImGuiCol_Button, c.surface);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c.ct);
    ImGui::PushStyleColor(ImGuiCol_Border, c.line);
    ImGui::PushStyleColor(ImGuiCol_Text, c.ink);

    if (ImGui::Button("##MenuBtn", ImVec2(44.0f, 44.0f))) {
        isNavRailExpanded_ = !isNavRailExpanded_;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isNavRailExpanded_ ? "Collapse Navigation Rail" : "Expand Navigation Rail");
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

    if (ImGui::InputTextWithHint("##HeaderSearch", "Search Mist (share, connect, devices...)", searchQuery_, IM_ARRAYSIZE(searchQuery_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string q = searchQuery_;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        if (q.find("host") != std::string::npos || q.find("share") != std::string::npos || q.find("cast") != std::string::npos) {
            navigateToScreen(DesktopScreen::Host);
            searchQuery_[0] = '\0';
        } else if (q.find("conn") != std::string::npos || q.find("join") != std::string::npos || q.find("code") != std::string::npos) {
            navigateToScreen(DesktopScreen::Connect);
            searchQuery_[0] = '\0';
        } else if (q.find("dev") != std::string::npos || q.find("pair") != std::string::npos) {
            navigateToScreen(DesktopScreen::Devices);
            searchQuery_[0] = '\0';
        } else if (q.find("net") != std::string::npos || q.find("wifi") != std::string::npos || q.find("bitrate") != std::string::npos || q.find("lag") != std::string::npos) {
            navigateToScreen(DesktopScreen::Network);
            searchQuery_[0] = '\0';
        } else if (q.find("live") != std::string::npos || q.find("stream") != std::string::npos) {
            navigateToScreen(DesktopScreen::LiveSession);
            searchQuery_[0] = '\0';
        } else if (q.find("home") != std::string::npos) {
            navigateToScreen(DesktopScreen::Home);
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
    if (isHostingActive_ || isConnecting_ || clientProcessPid_ != 0) {
        renderChip("Streaming", 1);
    } else {
        renderChip("Ready", 0);
    }

    // 4. Circular Devices Button (with badge "1" if pending)
    ImGui::SameLine(0, 12.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.0f);
    ImVec2 devBtnPos = ImGui::GetCursorScreenPos();

    if (ImGui::Button("##TopDev", ImVec2(44.0f, 44.0f))) {
        navigateToScreen(DesktopScreen::Devices);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Paired Devices & Permissions");
    }
    renderVectorIcon("devices", ImVec2(devBtnPos.x + 22.0f, devBtnPos.y + 22.0f), 22.0f, ImGui::ColorConvertFloat4ToU32(c.ink));

    // Badge "1" at top-right corner of device button
    if (incomingRequestActive_ || isDeviceApprovalPending_) {
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
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Settings & Video Quality");
    }
    renderVectorIcon("settings", ImVec2(gearPos.x + 22.0f, gearPos.y + 22.0f), 22.0f, ImGui::ColorConvertFloat4ToU32(c.ink));

    // 6. Circular Theme Toggle Button
    ImGui::SameLine(0, 8.0f);
    ImVec2 themePos = ImGui::GetCursorScreenPos();
    if (ImGui::Button("##TopTheme", ImVec2(44.0f, 44.0f))) {
        toggleTheme();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip((currentTheme_ == tokens::ThemeMode::Light) ? "Switch to Dark Theme" : "Switch to Light Theme");
    }
    renderVectorIcon((currentTheme_ == tokens::ThemeMode::Light) ? "dark_mode" : "light_mode",
                     ImVec2(themePos.x + 22.0f, themePos.y + 22.0f), 22.0f, ImGui::ColorConvertFloat4ToU32(c.ink));

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImGui::EndGroup();
}

void DesktopApplication::renderGuiNavRail() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    float targetRailW = isNavRailExpanded_ ? 272.0f : 76.0f;
    float railWidth = getAnim("nav_rail_w", targetRailW, 16.0f);
    bool isCollapsed = (railWidth < 120.0f);

    // Transparent child window so ambient wallpaper shines through
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("NavRail", ImVec2(railWidth, 0), false, ImGuiWindowFlags_NoScrollbar);

    struct NavItem {
        DesktopScreen screen;
        const char* label;
        const char* icon;
        bool hasBadge;
    };

    std::vector<NavItem> items = {
        { DesktopScreen::Home,        "Home",        "home",          false },
        { DesktopScreen::Host,        "Wi-Fi Share", "cast",          false },
        { DesktopScreen::Connect,     "Connect",     "sports_esports",false },
        { DesktopScreen::Devices,     "Devices",     "devices",       incomingRequestActive_ || isDeviceApprovalPending_ },
        { DesktopScreen::Network,     "Network",     "network_check", false },
    };

    if (isHostingActive_ || isConnecting_ || clientProcessPid_ != 0 || hostProcessPid_ != 0 || currentScreen_ == DesktopScreen::LiveSession) {
        items.push_back({ DesktopScreen::LiveSession, "Live Stream", "tv", false });
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tokens::MintShapes::Pill);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(isCollapsed ? 12.0f : 20.0f, 14.0f));

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
        float btnW = isCollapsed ? 52.0f : (railWidth - 24.0f);
        std::string btnId = "##NavBtn_" + std::string(item.icon);
        std::string btnText = isCollapsed ? "" : ("        " + std::string(item.label));

        if (fontBold_) ImGui::PushFont(fontBold_);
        if (ImGui::Button((btnText.empty() ? btnId.c_str() : btnText.c_str()), ImVec2(btnW, 54.0f))) {
            navigateToScreen(item.screen);
        }
        if (fontBold_) ImGui::PopFont();

        if (isCollapsed && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", item.label);
        }

        // Render navigation icon
        ImU32 iconColor = isSelected ? ImGui::ColorConvertFloat4ToU32(c.onCt) : ImGui::ColorConvertFloat4ToU32(c.mute);
        float iconX = isCollapsed ? (pStart.x + btnW * 0.5f) : (pStart.x + 28.0f);
        renderVectorIcon(item.icon, ImVec2(iconX, pStart.y + 27.0f), 24.0f, iconColor);

        if (item.hasBadge) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float bx = isCollapsed ? (pStart.x + btnW - 12.0f) : (pStart.x + railWidth - 56.0f);
            float by = isCollapsed ? (pStart.y + 12.0f) : (pStart.y + 27.0f);
            drawList->AddRectFilled(ImVec2(bx - 10.0f, by - 10.0f), ImVec2(bx + 10.0f, by + 10.0f), ImGui::ColorConvertFloat4ToU32(c.mint), 99.0f);
            drawList->AddText(ImVec2(bx - 3.5f, by - 7.0f), ImGui::ColorConvertFloat4ToU32(c.onMint), "1");
        }

        ImGui::PopStyleColor(5);
        ImGui::Spacing();
    }
    ImGui::PopStyleVar(2);

    // Footnote at bottom of rail
    float availY = ImGui::GetContentRegionAvail().y;
    if (availY > 60.0f && !isCollapsed) {
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
    ImGui::TextColored(c.mute, "Host this PC or join another one over Wi-Fi. Simple, single-use pairing codes.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Spacing();

    // 2. Latest Sessions Section
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Latest sessions");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    auto trusted = pairingManager_.getTrustedDevices();
    if (trusted.empty()) {
        BeginCardContainer("NoSessionsCard", c, ImVec2(availW, 110.0f), 24.0f);
        ImVec2 emptyPos = ImGui::GetCursorScreenPos();
        renderVectorIcon("devices", ImVec2(emptyPos.x + 24.0f, emptyPos.y + 24.0f), 28.0f, ImGui::ColorConvertFloat4ToU32(c.mute));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 64.0f);
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "No Previous Sessions");
        if (fontBold_) ImGui::PopFont();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 64.0f);
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "Pair a device using a 6-character code to see your trusted connections here.");
        if (fontSmall_) ImGui::PopFont();
        ImGui::SameLine(availW - 190.0f);
        if (renderPillButton("Start Sharing", true, false, ImVec2(150.0f, 38.0f), "cast")) {
            navigateToScreen(DesktopScreen::Host);
        }
        EndCardContainer();
    } else {
        float carCardW = std::max(220.0f, (availW - 32.0f) / 3.0f);
        size_t count = std::min(trusted.size(), size_t{3});
        for (size_t i = 0; i < count; ++i) {
            if (i > 0) ImGui::SameLine(0, 14.0f);
            BeginCardContainer(("SessionCard" + std::to_string(i)).c_str(), c, ImVec2(carCardW, 126.0f), 24.0f);
            if (fontBold_) ImGui::PushFont(fontBold_);
            ImGui::TextColored(c.ink, "%s", trusted[i].deviceName.c_str());
            if (fontBold_) ImGui::PopFont();

            if (fontSmall_) ImGui::PushFont(fontSmall_);
            ImGui::TextColored(c.mute, "Trusted Device * %s", trusted[i].deviceId.substr(0, std::min(size_t{8}, trusted[i].deviceId.size())).c_str());
            ImGui::Spacing();
            std::string permStr;
            if (trusted[i].permissions.allowKeyboard || trusted[i].permissions.allowMouse) permStr += "Mouse/Keys ";
            if (trusted[i].permissions.allowGamepad) permStr += "Gamepad ";
            if (permStr.empty()) permStr = "Audio/Video Only";
            ImGui::TextColored(c.acc, "%s", permStr.c_str());
            if (fontSmall_) ImGui::PopFont();
            EndCardContainer();
        }
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
        { "Wi-Fi Screen Share", "Share your desktop and games with a 6-character code.", DesktopScreen::Host,    "cast" },
        { "Connect with Code",  "Join an active stream using its 6-character pairing code.", DesktopScreen::Connect, "sports_esports" },
        { "Paired Devices",     "Manage trusted devices and control remote input.",          DesktopScreen::Devices, "devices" },
        { "Wi-Fi & Network",    "Inspect live Wi-Fi link speed, ping, and throughput.",     DesktopScreen::Network, "network_check" },
    };

    for (int i = 0; i < 4; ++i) {
        if (i % 2 == 1) ImGui::SameLine(0, 16.0f);
        else if (i > 0) ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, c.ct2);
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, tokens::MintShapes::Tile); // 24px
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 22.0f));

        std::string tileId = "Tile" + std::to_string(i);
        ImGui::BeginChild(tileId.c_str(), ImVec2(tileW, 152.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // 52x52 rounded 16px mint icon box
        ImVec2 curPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(curPos, ImVec2(curPos.x + 52.0f, curPos.y + 52.0f), ImGui::ColorConvertFloat4ToU32(c.mint), 16.0f);

        // Render the exact Material icon inside the mint box
        renderVectorIcon(tiles[i].icon, ImVec2(curPos.x + 26.0f, curPos.y + 26.0f), 26.0f, ImGui::ColorConvertFloat4ToU32(c.onMint));

        ImGui::Dummy(ImVec2(52.0f, 52.0f));
        ImGui::SameLine(0, 16.0f);
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
            navigateToScreen(tiles[i].target);
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
        std::string chipText;
        int status;
    };

    std::string videoEncoderStr = liveSystemHardware_.gpuName;
    if (liveSystemHardware_.hasNvenc) {
        videoEncoderStr += " * Hardware NVENC";
    }

    std::string netStr;
    if (liveNetworkStatus_.isWifi && !liveNetworkStatus_.wifiSsid.empty()) {
        netStr = "Wi-Fi: " + liveNetworkStatus_.wifiSsid;
        if (liveNetworkStatus_.wifiFrequencyGhz > 4.0f) {
            netStr += " (5 GHz)";
        }
    } else {
        netStr = liveNetworkStatus_.adapterName;
        if (liveNetworkStatus_.linkSpeedMbps > 0) {
            netStr += " (" + std::to_string(liveNetworkStatus_.linkSpeedMbps) + " Mbps)";
        }
    }

    const SysRow rows[] = {
        { "Video encoder",    videoEncoderStr, liveSystemHardware_.hasNvenc ? 1 : 2 },
        { "Graphics driver",  liveSystemHardware_.gpuDriver, 1 },
        { "Display session",  liveSystemHardware_.displayInfo, 1 },
        { "Audio output",     liveSystemHardware_.audioEndpoint, 1 },
        { "Active network",   netStr, 1 },
        { "Controller driver",liveSystemHardware_.gamepadStatus, liveSystemHardware_.hasGamepad ? 1 : 2 },
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
        ImGui::SameLine(rw - 400.0f);
        renderChip(rows[i].chipText.c_str(), rows[i].status);
        ImGui::Spacing();
    }

    EndCardContainer();
}

void DesktopApplication::renderGuiHostScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Wi-Fi Screen Share");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Share your desktop and games over Wi-Fi with instant, secure 6-character code pairing.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();

    // Wi-Fi Capability & Preflight Banner
    BeginCardContainer("WifiCapabilityBanner", c, ImVec2(0, 0), 20.0f);
    ImVec2 curPos = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(curPos, ImVec2(curPos.x + 40.0f, curPos.y + 40.0f), ImGui::ColorConvertFloat4ToU32(c.mint), 12.0f);
    renderVectorIcon("wifi", ImVec2(curPos.x + 20.0f, curPos.y + 20.0f), 22.0f, ImGui::ColorConvertFloat4ToU32(c.onMint));

    ImGui::Dummy(ImVec2(40.0f, 40.0f));
    ImGui::SameLine(0, 14.0f);
    ImGui::BeginGroup();
    if (fontBold_) ImGui::PushFont(fontBold_);
    if (liveNetworkStatus_.isWifi && !liveNetworkStatus_.wifiSsid.empty()) {
        ImGui::TextColored(c.ink, "Wi-Fi: %s (%d%% signal)", liveNetworkStatus_.wifiSsid.c_str(), liveNetworkStatus_.wifiSignalQualityPercent);
    } else {
        ImGui::TextColored(c.ink, "Direct Network: %s (%llu Mbps)", liveNetworkStatus_.adapterName.c_str(), static_cast<unsigned long long>(liveNetworkStatus_.linkSpeedMbps));
    }
    if (fontBold_) ImGui::PopFont();

    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "GPU Encoder: %s * Audio: %s", liveSystemHardware_.gpuName.c_str(), liveSystemHardware_.audioEndpoint.c_str());
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    float rw = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(rw - 170.0f);
    renderChip(liveSystemHardware_.hasNvenc ? "Hardware Ready" : "Software Fallback", liveSystemHardware_.hasNvenc ? 1 : 2);

    EndCardContainer();

    ImGui::Spacing();
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float colW = std::max(300.0f, (availW - 16.0f) / 2.0f);

    // Left Column: Wi-Fi Sharing Session
    BeginCardContainer("HostStreamCard", c, ImVec2(colW, 460.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Wi-Fi Sharing");
    if (fontTitle_) ImGui::PopFont();
    ImGui::SameLine(colW - 140.0f);
    renderChip(isHostingActive_ ? "Streaming" : "Ready", isHostingActive_ ? 1 : 0);

    ImGui::Spacing();
    ImGui::Spacing();

    if (isHostingActive_) {
        // Pairing Code Display (6 Large Digits)
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "PAIRING CODE FOR CLIENT DEVICE");
        if (fontSmall_) ImGui::PopFont();
        ImGui::Spacing();

        std::string displayCode;
        for (char ch : pairingCode_) {
            if (ch != '-') displayCode += ch;
        }

        float spanW = std::min(52.0f, (colW - 80.0f) / 6.0f);
        float spanH = 68.0f;

        for (size_t i = 0; i < 6 && i < displayCode.size(); ++i) {
            if (i > 0) ImGui::SameLine(0, 8.0f);
            ImVec2 p = ImGui::GetCursorScreenPos();

            ImVec4 boxBg = c.ct;
            boxBg.w = 0.80f + 0.20f * codePulseEffect_;
            drawList->AddRectFilled(p, ImVec2(p.x + spanW, p.y + spanH), ImGui::ColorConvertFloat4ToU32(boxBg), 16.0f);
            drawList->AddRect(p, ImVec2(p.x + spanW, p.y + spanH), ImGui::ColorConvertFloat4ToU32(c.mint), 16.0f, 0, 1.6f);

            char digitStr[2] = { displayCode[i], '\0' };
            if (fontCode_) ImGui::PushFont(fontCode_);
            ImVec2 ts = ImGui::CalcTextSize(digitStr);
            drawList->AddText(ImVec2(p.x + (spanW - ts.x) * 0.5f, p.y + (spanH - ts.y) * 0.5f), ImGui::ColorConvertFloat4ToU32(c.onCt), digitStr);
            if (fontCode_) ImGui::PopFont();

            ImGui::Dummy(ImVec2(spanW, spanH));
        }

        ImGui::Spacing();
        int mins = codeRemainingSeconds_ / 60;
        int secs = codeRemainingSeconds_ % 60;
        char expBuf[64];
        snprintf(expBuf, sizeof(expBuf), "Code expires in %02d:%02d * Single-use security code.", mins, secs);

        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.acc, "%s", expBuf);
        if (fontSmall_) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Spacing();

        if (renderPillButton("Copy Code", false, false, ImVec2(120.0f, 40.0f), "content_copy")) {
            ImGui::SetClipboardText(pairingCode_.c_str());
            showToast("Pairing code copied to clipboard");
        }
        ImGui::SameLine(0, 10.0f);
        if (renderPillButton("New Code", false, false, ImVec2(120.0f, 40.0f), "refresh")) {
            auto codeRes = pairingManager_.generatePairingCode(300);
            if (codeRes.hasValue()) {
                pairingCode_ = codeRes.value();
            } else {
                pairingCode_ = generateRandomPairingCode();
            }
            codeRemainingSeconds_ = 300;
            codeTimer_ = 0.0f;
            showToast("New code ready: " + pairingCode_);
        }
        ImGui::SameLine(0, 10.0f);
        if (renderPillButton("Stop Sharing", false, true, ImVec2(130.0f, 40.0f), "stop")) {
            terminateHostProcess();
            isHostingActive_ = false;
            showToast("Wi-Fi sharing stopped");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Live Telemetry
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

        ImGui::Spacing();
        if (renderPillButton("Open Viewport", true, false, ImVec2(180.0f, 42.0f), "tv")) {
            navigateToScreen(DesktopScreen::LiveSession);
        }
    } else {
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(c.mute, "Click below to begin broadcasting your screen over Wi-Fi. Mist will generate a single-use pairing code for your client device.");
        if (fontRegular_) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Spacing();

        renderM3Switch("HostStreamAudio", "Stream game audio", &hostAudioStream_, "Transmits low-latency stereo game sound via WASAPI loopback");
        ImGui::Spacing();
        renderM3Switch("HostRemoteInput", "Allow remote input", &hostAllowRemoteInput_, "Keyboard, mouse, and gamepad controls. Host mouse always wins.");

        ImGui::Spacing();
        ImGui::Spacing();

        if (renderPillButton("Start Wi-Fi Sharing", true, false, ImVec2(240.0f, 48.0f), "cast")) {
            auto codeRes = pairingManager_.generatePairingCode(300);
            if (codeRes.hasValue()) {
                pairingCode_ = codeRes.value();
            } else {
                pairingCode_ = generateRandomPairingCode();
            }
            codeRemainingSeconds_ = 300;
            codeTimer_ = 0.0f;
            if (launchHostProcess(pairingCode_, hostSignalingServer_, hostAudioStream_, hostAllowRemoteInput_)) {
                isHostingActive_ = true;
                showToast("Wi-Fi sharing started with code " + pairingCode_);
            }
        }
    }

    EndCardContainer();

    // Right Column: Incoming Approvals & Connected Devices
    ImGui::SameLine(0, 16.0f);
    BeginCardContainer("HostApprovalCard", c, ImVec2(colW, 460.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Device Approvals & Permissions");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    auto pendingList = pairingManager_.getPendingRequests();
    if (!pendingList.empty() || incomingRequestActive_) {
        std::string reqDevId = !pendingList.empty() ? pendingList[0].deviceId : pendingDeviceId_;
        std::string reqDevName = !pendingList.empty() ? pendingList[0].deviceName : pendingDeviceName_;

        ImVec2 reqPos = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(reqPos, ImVec2(reqPos.x + colW - 48.0f, reqPos.y + 60.0f), ImGui::ColorConvertFloat4ToU32(c.ct), 16.0f);

        ImGui::SetCursorScreenPos(ImVec2(reqPos.x + 16.0f, reqPos.y + 10.0f));
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.onCt, "%s", reqDevName.c_str());
        if (fontBold_) ImGui::PopFont();
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.acc, "Pairing code verified. Awaiting host approval.");
        if (fontSmall_) ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(reqPos.x, reqPos.y + 70.0f));
        ImGui::Spacing();

        renderM3Switch("HostApprAudio", "Allow Game Audio", &pendingPermissions_.allowAudio, "Device receives stereo game audio");
        ImGui::Spacing();
        renderM3Switch("HostApprInput", "Allow Mouse & Keyboard", &pendingPermissions_.allowKeyboard, "Remote client can send keystrokes and clicks");
        ImGui::Spacing();
        renderM3Switch("HostApprPad", "Allow Gamepad", &pendingPermissions_.allowGamepad, "Virtual XInput controller mapping");

        ImGui::Spacing();
        ImGui::Spacing();

        if (renderPillButton("Approve Device", true, false, ImVec2(160.0f, 42.0f), "check_circle")) {
            security::DevicePermissions perms;
            perms.allowAudio = pendingPermissions_.allowAudio;
            perms.allowKeyboard = pendingPermissions_.allowKeyboard;
            perms.allowMouse = pendingPermissions_.allowKeyboard;
            perms.allowGamepad = pendingPermissions_.allowGamepad;
            (void)pairingManager_.approvePairingRequest(reqDevId, perms);
            incomingRequestActive_ = false;
            showToast(reqDevName + " approved and connected!");
        }
        ImGui::SameLine(0, 10.0f);
        if (renderPillButton("Deny & Block", false, true, ImVec2(130.0f, 42.0f), "close")) {
            (void)pairingManager_.rejectPairingRequest(reqDevId);
            incomingRequestActive_ = false;
            showToast("Pairing request rejected");
        }
    } else {
        auto trustedList = pairingManager_.getTrustedDevices();
        if (trustedList.empty()) {
            if (fontRegular_) ImGui::PushFont(fontRegular_);
            ImGui::TextColored(c.mute, "No devices waiting for approval.");
            if (fontRegular_) ImGui::PopFont();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (fontRegular_) ImGui::PushFont(fontRegular_);
            ImGui::TextColored(c.mute, "No trusted devices yet. Connect a client using your 6-character sharing code.");
            if (fontRegular_) ImGui::PopFont();
        } else {
            if (fontBold_) ImGui::PushFont(fontBold_);
            ImGui::TextColored(c.ink, "Trusted Devices");
            if (fontBold_) ImGui::PopFont();
            ImGui::Spacing();

            std::string toRevokeId;
            for (size_t i = 0; i < trustedList.size(); ++i) {
                ImGui::AlignTextToFramePadding();
                if (fontBold_) ImGui::PushFont(fontBold_);
                ImGui::TextColored(c.ink, "%s", trustedList[i].deviceName.c_str());
                if (fontBold_) ImGui::PopFont();

                std::string kbdId = "KbdSwitch_" + std::to_string(i);
                bool kbdVal = trustedList[i].permissions.allowKeyboard;
                if (renderM3Switch(kbdId.c_str(), "Mouse/Keys", &kbdVal, nullptr, 140.0f)) {
                    auto p = trustedList[i].permissions;
                    p.allowKeyboard = kbdVal;
                    p.allowMouse = kbdVal;
                    (void)pairingManager_.updateDevicePermissions(trustedList[i].deviceId, p);
                    showToast("Permission updated");
                }

                ImGui::SameLine(0, 16.0f);
                std::string padId = "PadSwitch_" + std::to_string(i);
                bool padVal = trustedList[i].permissions.allowGamepad;
                if (renderM3Switch(padId.c_str(), "Gamepad", &padVal, nullptr, 130.0f)) {
                    auto p = trustedList[i].permissions;
                    p.allowGamepad = padVal;
                    (void)pairingManager_.updateDevicePermissions(trustedList[i].deviceId, p);
                    showToast("Permission updated");
                }

                ImGui::SameLine(0, 16.0f);
                std::string remId = "Revoke##" + std::to_string(i);
                if (renderPillButton(remId.c_str(), false, true, ImVec2(80.0f, 32.0f))) {
                    toRevokeId = trustedList[i].deviceId;
                }
                ImGui::Spacing();
            }

            if (!toRevokeId.empty()) {
                (void)pairingManager_.revokeDevice(toRevokeId);
                showToast("Device access revoked");
            }
        }
    }

    EndCardContainer();
}

void DesktopApplication::renderGuiConnectScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);

    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Connect to a Device");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Enter the 6-character code displayed on the host screen to begin streaming.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();

    float availW = ImGui::GetContentRegionAvail().x;
    float colW = std::max(300.0f, (availW - 16.0f) / 2.0f);

    // Left Column: Enter Pairing Code Card
    BeginCardContainer("ConnectFormCard", c, ImVec2(colW, 460.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Enter Pairing Code");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Ask the host for the 6-character pairing code shown on their Wi-Fi Share screen.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Spacing();

    // 6-Character Code Input Field with Paste Button
    float inputFieldW = colW - 140.0f;
    renderM3TextField("##ClientPairingCode", "6-CHARACTER PAIRING CODE", clientPairingCode_, IM_ARRAYSIZE(clientPairingCode_), "e.g. 7DK4LM", inputFieldW);
    ImGui::SameLine(0, 12.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 24.0f);
    if (renderPillButton("Paste", false, false, ImVec2(90.0f, 44.0f), "content_paste")) {
        const char* clip = ImGui::GetClipboardText();
        if (clip) {
            std::string clean;
            for (const char* p = clip; *p; ++p) {
                if (isalnum(static_cast<unsigned char>(*p))) {
                    clean += static_cast<char>(toupper(*p));
                    if (clean.size() == 6) break;
                }
            }
            if (!clean.empty()) {
                strncpy_s(clientPairingCode_, clean.c_str(), sizeof(clientPairingCode_) - 1);
                showToast("Pasted code: " + clean);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    renderM3Switch("ConnHearAudio", "Hear game audio", &connectHearAudio_, "Transmits low-delay stereo game sound");
    ImGui::Spacing();
    renderM3Switch("ConnSendInput", "Send controller and mouse input", &connectSendInput_, "Keyboard, mouse, and gamepad controls");

    ImGui::Spacing();
    ImGui::Spacing();

    if (!isConnecting_) {
        if (renderPillButton("Connect", true, false, ImVec2(180.0f, 48.0f), "play_arrow")) {
            std::string code(clientPairingCode_);
            code.erase(std::remove_if(code.begin(), code.end(), ::isspace), code.end());
            std::transform(code.begin(), code.end(), code.begin(), ::toupper);

            if (code.size() != 6 && code.size() != 7) {
                showToast("Please enter a valid 6-character pairing code");
            } else {
                std::string formattedCode = code;
                if (code.size() == 6) {
                    formattedCode = code.substr(0, 3) + "-" + code.substr(3, 3);
                }
                static const std::string validChars = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ-";
                bool validAlphabet = true;
                for (char ch : formattedCode) {
                    if (validChars.find(ch) == std::string::npos) {
                        validAlphabet = false;
                        break;
                    }
                }
                if (!validAlphabet) {
                    showToast("Code contains invalid characters (0, O, 1, I not permitted)");
                    connectStatusMsg_ = "Invalid pairing code characters.";
                } else {
                    isConnecting_ = true;
                    connectStep_ = 1;
                    connectStepTimer_ = 0.0f;
                    connectStatusMsg_ = "Connecting to host over Wi-Fi...";

                    security::PairingRequest req;
                    req.deviceId = "client-" + std::to_string(GetCurrentProcessId());
                    req.deviceName = "Client Workstation (" + liveNetworkStatus_.adapterName + ")";
                    req.pairingCode = formattedCode;
                    auto subRes = pairingManager_.submitPairingRequest(req);
                    if (!subRes.hasValue()) {
                        isConnecting_ = false;
                        connectStep_ = 0;
                        connectStatusMsg_ = subRes.error().message();
                        showToast(subRes.error().message());
                    } else {
                        if (launchClientProcess(formattedCode, connectSignalingServer_, connectHearAudio_, connectSendInput_)) {
                            showToast("Connecting with code " + formattedCode);
                        }
                    }
                }
            }
        }
    } else {
        if (renderPillButton("Cancel Connection", false, true, ImVec2(180.0f, 44.0f), "stop")) {
            terminateClientProcess();
            isConnecting_ = false;
            connectStep_ = 0;
            connectStatusMsg_ = "Connection cancelled.";
            showToast("Connection cancelled");
        }
    }

    EndCardContainer();

    // Right Column: Connection Progress Steps Card
    ImGui::SameLine(0, 16.0f);
    BeginCardContainer("ConnectStepsCard", c, ImVec2(colW, 460.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Connection Progress");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    const char* steps[] = {
        "Verify Wi-Fi network link",
        "Authenticate 6-character pairing code",
        "Open encrypted WebRTC stream",
        "Receive first video frame"
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
    BeginCardContainer("PairingCodeCard", c, ImVec2(colW, 320.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Pairing code");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    // 6 Large Digits in Rounded Pill Containers
    std::string displayCode;
    for (char ch : pairingCode_) {
        if (ch != '-') displayCode += ch;
    }
    float spanW = std::min(58.0f, (colW - 80.0f) / 6.0f);
    float spanH = 72.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < 6 && i < displayCode.size(); ++i) {
        if (i > 0) ImGui::SameLine(0, 8.0f);
        ImVec2 p = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(p, ImVec2(p.x + spanW, p.y + spanH), ImGui::ColorConvertFloat4ToU32(c.ct), 16.0f);

        char digitStr[2] = { displayCode[i], '\0' };
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
    if (renderPillButton("New code", true, false, ImVec2(130.0f, 42.0f))) {
        if (codeLockSeconds_ <= 0) {
            auto codeRes = pairingManager_.generatePairingCode(300);
            if (codeRes.hasValue()) {
                pairingCode_ = codeRes.value();
                wrongCodeFails_ = 0;
                codeHintMsg_ = "Read this code to the person pairing. It works once.";
                showToast("New code ready: " + pairingCode_);
            }
        }
    }
    ImGui::SameLine(0, 10.0f);
    if (renderPillButton("Try a wrong code", false, false, ImVec2(170.0f, 42.0f))) {
        if (codeLockSeconds_ <= 0) {
            security::PairingRequest req;
            req.deviceId = "test-bad-client";
            req.deviceName = "Untrusted Test Device";
            req.pairingCode = "BAD-999";
            auto badRes = pairingManager_.submitPairingRequest(req);
            if (!badRes.hasValue()) {
                wrongCodeFails_++;
                if (badRes.error().code() == common::ErrorCode::PermissionDenied || wrongCodeFails_ >= 5) {
                    codeLockSeconds_ = 60;
                    codeHintMsg_ = "Too many wrong codes. Try again in 60 seconds.";
                    triggerError("Device Pairing Security Lockout",
                                 badRes.error().message(),
                                 "1. Wait 60 seconds for the lockout countdown to expire.\n"
                                 "2. Click 'New code' to generate a fresh 6-digit one-time code.\n"
                                 "3. Verify the client device is entering the matching code.",
                                 "ERR_AUTH_PAIRING_LOCKOUT",
                                 DesktopScreen::Devices);
                } else {
                    codeHintMsg_ = "Wrong code. " + std::to_string(std::max(0, 5 - wrongCodeFails_)) + " attempts left before a 60-second pause.";
                    showToast(badRes.error().message());
                }
            }
        }
    }

    EndCardContainer();

    // Right: Waiting for Approval Card
    ImGui::SameLine(0, 16.0f);
    BeginCardContainer("ApprovalCard", c, ImVec2(colW, 320.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Waiting for approval");
    if (fontTitle_) ImGui::PopFont();
    ImGui::Spacing();

    auto devPending = pairingManager_.getPendingRequests();
    if (!devPending.empty() || isDeviceApprovalPending_) {
        std::string pId = !devPending.empty() ? devPending[0].deviceId : "test-pending";
        std::string pName = !devPending.empty() ? devPending[0].deviceName : "Aiden's Laptop";

        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "%s", pName.c_str());
        if (fontBold_) ImGui::PopFont();
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "Used a code just now");
        if (fontSmall_) ImGui::PopFont();

        ImGui::Spacing();
        if (renderPillButton("Approve", true, false, ImVec2(110.0f, 42.0f))) {
            security::DevicePermissions perms;
            perms.allowAudio = true;
            perms.allowKeyboard = false;
            perms.allowMouse = false;
            perms.allowGamepad = false;
            (void)pairingManager_.approvePairingRequest(pId, perms);
            isDeviceApprovalPending_ = false;
            showToast(pName + " approved. Remote input is disabled.");
        }
        ImGui::SameLine(0, 10.0f);
        if (renderPillButton("Deny", false, false, ImVec2(90.0f, 42.0f))) {
            (void)pairingManager_.rejectPairingRequest(pId);
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
    auto devTrusted = pairingManager_.getTrustedDevices();
    if (devTrusted.empty()) {
        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(c.mute, "No trusted devices yet. Create a pairing code to add one.");
        if (fontRegular_) ImGui::PopFont();
    } else {
        std::string toRemoveId;
        for (size_t i = 0; i < devTrusted.size(); ++i) {
            if (i > 0) {
                ImGui::Separator();
                ImGui::Spacing();
            }
            ImGui::BeginGroup();
            if (fontBold_) ImGui::PushFont(fontBold_);
            ImGui::TextColored(c.ink, "%s", devTrusted[i].deviceName.c_str());
            if (fontBold_) ImGui::PopFont();
            if (fontSmall_) ImGui::PushFont(fontSmall_);
            ImGui::TextColored(c.mute, "Device ID: %s", devTrusted[i].deviceId.substr(0, std::min(size_t{8}, devTrusted[i].deviceId.size())).c_str());
            if (fontSmall_) ImGui::PopFont();
            ImGui::EndGroup();

            float rowW = ImGui::GetContentRegionAvail().x;
            ImGui::SameLine(rowW - 380.0f);
            std::string kbdId = "DevKbdSwitch_" + std::to_string(i);
            bool kbdVal = devTrusted[i].permissions.allowKeyboard;
            if (renderM3Switch(kbdId.c_str(), "Mouse/Keys", &kbdVal, nullptr, 140.0f)) {
                auto p = devTrusted[i].permissions;
                p.allowKeyboard = kbdVal;
                p.allowMouse = kbdVal;
                (void)pairingManager_.updateDevicePermissions(devTrusted[i].deviceId, p);
                showToast("Permission updated");
            }

            ImGui::SameLine(0, 16.0f);
            std::string padId = "DevPadSwitch_" + std::to_string(i);
            bool padVal = devTrusted[i].permissions.allowGamepad;
            if (renderM3Switch(padId.c_str(), "Gamepad", &padVal, nullptr, 130.0f)) {
                auto p = devTrusted[i].permissions;
                p.allowGamepad = padVal;
                (void)pairingManager_.updateDevicePermissions(devTrusted[i].deviceId, p);
                showToast("Permission updated");
            }

            ImGui::SameLine(0, 16.0f);
            std::string remId = "Remove##" + std::to_string(i);
            if (renderPillButton(remId.c_str(), false, true, ImVec2(80.0f, 32.0f))) {
                toRemoveId = devTrusted[i].deviceId;
            }
            ImGui::Spacing();
        }

        if (!toRemoveId.empty()) {
            (void)pairingManager_.revokeDevice(toRemoveId);
            showToast("Device removed");
        }
    }
    EndCardContainer();
}

void DesktopApplication::renderGuiNetworkScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);

    if (fontDisplay_) ImGui::PushFont(fontDisplay_);
    ImGui::TextColored(c.ink, "Network & Wi-Fi Monitor");
    if (fontDisplay_) ImGui::PopFont();

    if (fontRegular_) ImGui::PushFont(fontRegular_);
    ImGui::TextColored(c.mute, "Live hardware network telemetry probed in real-time via Windows WlanAPI, IP Helper, and ICMP Ping.");
    if (fontRegular_) ImGui::PopFont();
    ImGui::Spacing();

    // 1. Live Active Adapter & Hardware Link Card
    BeginCardContainer("NetworkAdapterCard", c, ImVec2(0, 0), 28.0f);

    float cardW = ImGui::GetContentRegionAvail().x;
    ImVec2 curPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 48x48 icon badge
    drawList->AddRectFilled(curPos, ImVec2(curPos.x + 48.0f, curPos.y + 48.0f), ImGui::ColorConvertFloat4ToU32(c.mint), 16.0f);
    renderVectorIcon(liveNetworkStatus_.isWifi ? "wifi" : "network_check", ImVec2(curPos.x + 24.0f, curPos.y + 24.0f), 24.0f, ImGui::ColorConvertFloat4ToU32(c.onMint));

    ImGui::Dummy(ImVec2(48.0f, 48.0f));
    ImGui::SameLine(0, 16.0f);

    ImGui::BeginGroup();
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "%s", liveNetworkStatus_.adapterName.c_str());
    if (fontTitle_) ImGui::PopFont();

    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "%s * %s", liveNetworkStatus_.connectionType.c_str(), liveNetworkStatus_.isWifi ? "Wireless RF" : "Direct Physical Ethernet");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(cardW - 200.0f);
    int qosStatusType = (liveNetworkStatus_.qosState.find("Optimal") != std::string::npos) ? 1 :
                        ((liveNetworkStatus_.qosState.find("Stable") != std::string::npos) ? 1 : 2);
    renderChip(liveNetworkStatus_.qosState.c_str(), qosStatusType);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    // 4 Details Columns
    float colW = cardW / 4.0f;

    // Col 1: IP & Gateway
    ImGui::BeginGroup();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "IPv4 ADDRESS");
    if (fontSmall_) ImGui::PopFont();
    if (fontBold_) ImGui::PushFont(fontBold_);
    ImGui::TextColored(c.ink, "%s", liveNetworkStatus_.ipv4Address.c_str());
    if (fontBold_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "Gateway %s", liveNetworkStatus_.gatewayAddress.c_str());
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    // Col 2: Link Speed
    ImGui::SameLine(colW);
    ImGui::BeginGroup();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "LINK SPEED");
    if (fontSmall_) ImGui::PopFont();
    if (fontBold_) ImGui::PushFont(fontBold_);
    ImGui::TextColored(c.ink, "%llu Mbps", static_cast<unsigned long long>(liveNetworkStatus_.linkSpeedMbps));
    if (fontBold_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "%s", liveNetworkStatus_.isWifi ? "Negotiated RF Link" : "Full Duplex Physical Link");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    // Col 3: Signal Quality / Cable Quality
    ImGui::SameLine(colW * 2.0f);
    ImGui::BeginGroup();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, liveNetworkStatus_.isWifi ? "WI-FI SIGNAL" : "CONNECTION QUALITY");
    if (fontSmall_) ImGui::PopFont();
    if (fontBold_) ImGui::PushFont(fontBold_);
    if (liveNetworkStatus_.isWifi) {
        ImGui::TextColored(c.ink, "%d%% (%d dBm)", liveNetworkStatus_.wifiSignalQualityPercent, liveNetworkStatus_.wifiRssiDbm);
    } else {
        ImGui::TextColored(c.ink, "100%% (Optimal)");
    }
    if (fontBold_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "%s", liveNetworkStatus_.isWifi ? liveNetworkStatus_.wifiStandard.c_str() : "Zero Signal Attenuation");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    // Col 4: Radio Band / Interference
    ImGui::SameLine(colW * 3.0f);
    ImGui::BeginGroup();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, liveNetworkStatus_.isWifi ? "RADIO FREQUENCY" : "RF INTERFERENCE");
    if (fontSmall_) ImGui::PopFont();
    if (fontBold_) ImGui::PushFont(fontBold_);
    if (liveNetworkStatus_.isWifi && liveNetworkStatus_.wifiFrequencyGhz > 0.0f) {
        ImGui::TextColored(c.ink, "%.3f GHz (Ch %d)", liveNetworkStatus_.wifiFrequencyGhz, liveNetworkStatus_.wifiChannel);
    } else if (liveNetworkStatus_.isWifi) {
        ImGui::TextColored(c.ink, "2.4 / 5 GHz");
    } else {
        ImGui::TextColored(c.ink, "Immune (0 dBm noise)");
    }
    if (fontBold_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "%s", liveNetworkStatus_.isWifi ? "WLAN AutoConfig Active" : "Shielded Twisted Pair");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    EndCardContainer();

    ImGui::Spacing();
    ImGui::Spacing();

    // 2. Real-Time Video & Network Throughput Graph Card
    BeginCardContainer("NetworkChartCard", c, ImVec2(0, 390.0f), 28.0f);
    if (fontTitle_) ImGui::PushFont(fontTitle_);
    ImGui::TextColored(c.ink, "Live Network Throughput & Latency");
    if (fontTitle_) ImGui::PopFont();
    ImGui::SameLine(cardW - 200.0f);
    int chartQosType = (networkQosState_.find("Optimal") != std::string::npos || networkQosState_.find("Stable") != std::string::npos) ? 1 :
                       ((networkQosState_.find("Recovering") != std::string::npos || networkQosState_.find("Increasing") != std::string::npos) ? 2 : 3);
    renderChip(networkQosState_.c_str(), chartQosType);

    ImGui::Spacing();

    // Custom Bitrate Graph (SVG polygon/polyline equivalent)
    ImVec2 graphPos = ImGui::GetCursorScreenPos();
    float graphW = ImGui::GetContentRegionAvail().x;
    float graphH = 150.0f;

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

        float maxVal = 16.0f;
        for (float val : bitrateHistory_) {
            if (val > maxVal) maxVal = val;
        }

        float stepX = graphW / static_cast<float>(bitrateHistory_.size() - 1);
        for (size_t i = 0; i < bitrateHistory_.size(); ++i) {
            float val = std::min(bitrateHistory_[i], maxVal);
            float yNorm = val / maxVal;
            float px = graphPos.x + static_cast<float>(i) * stepX;
            float py = graphPos.y + graphH - (yNorm * (graphH - 24.0f) + 12.0f);
            pts.push_back(ImVec2(px, py));
        }

        // Polygon Fill
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

    // 4 Real Stat Counters
    float statColW = graphW / 4.0f;
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%.2f", networkBitrate_);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "Mbps throughput (Live)");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(statColW);
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%d", networkRtt_);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "ms Gateway ping");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(statColW * 2.0f);
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%d", liveNetworkStatus_.internetPingMs > 0 ? liveNetworkStatus_.internetPingMs : 24);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "ms Internet ping (8.8.8.8)");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(statColW * 3.0f);
    ImGui::BeginGroup();
    if (fontHeadline_) ImGui::PushFont(fontHeadline_);
    ImGui::TextColored(c.ink, "%d", networkJitter_);
    if (fontHeadline_) ImGui::PopFont();
    if (fontSmall_) ImGui::PushFont(fontSmall_);
    ImGui::TextColored(c.mute, "ms variance / jitter");
    if (fontSmall_) ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::Spacing();
    if (renderPillButton("Refresh Network Probe", false, false, ImVec2(240.0f, 44.0f), "refresh")) {
        probeLiveNetwork();
        showToast("Network telemetry probed");
    }

    EndCardContainer();

    ImGui::Spacing();

    // 3. Wi-Fi Access Points or Wired LAN Summary Card
    if (liveNetworkStatus_.isWifi || !liveNetworkStatus_.nearbyWifiList.empty()) {
        BeginCardContainer("WifiApsCard", c, ImVec2(0, 0), 28.0f);
        if (fontTitle_) ImGui::PushFont(fontTitle_);
        ImGui::TextColored(c.ink, "Nearby Wi-Fi Networks (WlanAPI Scan)");
        if (fontTitle_) ImGui::PopFont();
        ImGui::Spacing();

        for (size_t i = 0; i < liveNetworkStatus_.nearbyWifiList.size(); ++i) {
            const auto& ap = liveNetworkStatus_.nearbyWifiList[i];
            if (i > 0) {
                ImGui::Separator();
                ImGui::Spacing();
            }
            ImGui::AlignTextToFramePadding();
            if (fontBold_) ImGui::PushFont(fontBold_);
            ImGui::TextColored(c.ink, "%s", ap.ssid.c_str());
            if (fontBold_) ImGui::PopFont();

            float rw = ImGui::GetContentRegionAvail().x;
            ImGui::SameLine(rw - 220.0f);
            std::string apText = std::to_string(ap.signalPercent) + "% * " + std::to_string(ap.rssiDbm) + " dBm";
            renderChip(apText.c_str(), ap.signalPercent > 70 ? 1 : (ap.signalPercent > 40 ? 2 : 3));
            ImGui::Spacing();
        }

        EndCardContainer();
    } else {
        BeginCardContainer("WiredNetworkAdvantageCard", c, ImVec2(0, 0), 28.0f);
        if (fontTitle_) ImGui::PushFont(fontTitle_);
        ImGui::TextColored(c.ink, "Direct Wired Connection Advantage");
        if (fontTitle_) ImGui::PopFont();
        ImGui::Spacing();

        if (fontRegular_) ImGui::PushFont(fontRegular_);
        ImGui::TextColored(c.mute,
            "Your workstation is streaming over a dedicated physical network connection (%s).\n"
            "Wired connections provide sustained sub-millisecond latency (<1 ms), 0%% packet loss, and zero radio frequency jitter,\n"
            "enabling optimal 1080p60 and 4K60 low-latency remote game streaming.",
            liveNetworkStatus_.adapterName.c_str());
        if (fontRegular_) ImGui::PopFont();

        EndCardContainer();
    }
}

void DesktopApplication::renderStreamingHud() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float hudAlpha = getAnim("hud_alpha", showLiveStreamingHud_ ? 1.0f : 0.50f, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, hudAlpha);

    // Floating Glass HUD Pill at top of viewport
    float hudW = showLiveStreamingHud_ ? std::min(io.DisplaySize.x - 340.0f, 780.0f) : 260.0f;
    float hudH = showLiveStreamingHud_ ? 64.0f : 44.0f;
    float hudX = (io.DisplaySize.x - hudW) * 0.5f;
    float hudY = 24.0f;

    ImVec2 hudMin(hudX, hudY);
    ImVec2 hudMax(hudX + hudW, hudY + hudH);

    // Frosted glass background
    ImVec4 hudBg = c.surface;
    hudBg.w = 0.92f;
    drawList->AddRectFilled(hudMin, hudMax, ImGui::ColorConvertFloat4ToU32(hudBg), 32.0f);
    drawList->AddRect(hudMin, hudMax, ImGui::ColorConvertFloat4ToU32(c.line), 32.0f, 0, 1.5f);

    ImGui::SetCursorScreenPos(ImVec2(hudX + 20.0f, hudY + (hudH - 36.0f) * 0.5f));
    ImGui::BeginGroup();

    // 1. Live status indicator dot
    ImVec2 dotPos = ImGui::GetCursorScreenPos();
    drawList->AddCircleFilled(ImVec2(dotPos.x + 8.0f, dotPos.y + 16.0f), 6.0f, ImGui::ColorConvertFloat4ToU32(c.mint));
    ImGui::Dummy(ImVec2(18.0f, 32.0f));
    ImGui::SameLine(0, 8.0f);

    if (fontBold_) ImGui::PushFont(fontBold_);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(c.ink, "LIVE");
    if (fontBold_) ImGui::PopFont();

    ImGui::SameLine(0, 14.0f);

    if (showLiveStreamingHud_) {
        // Detailed metrics
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "1080p60 | %.1f Mbps | %d ms | Jitter %d ms | Loss %.0f%%",
                           networkBitrate_, networkRtt_, networkJitter_, networkLoss_);
        if (fontSmall_) ImGui::PopFont();

        ImGui::SameLine(0, 16.0f);

        // Quality Profile pill button in HUD
        const char* profName = "Adaptive";
        auto curProf = qualityManager_.getProfile();
        if (curProf == transport::qos::QualityProfile::Performance) profName = "Performance";
        else if (curProf == transport::qos::QualityProfile::Balanced) profName = "Balanced";
        else if (curProf == transport::qos::QualityProfile::Quality) profName = "Quality";

        if (renderPillButton(profName, false, false, ImVec2(96.0f, 30.0f))) {
            if (curProf == transport::qos::QualityProfile::Performance) {
                qualityManager_.setProfile(transport::qos::QualityProfile::Balanced);
                showToast("Profile set to Balanced (1080p60, 8 Mbps)");
            } else if (curProf == transport::qos::QualityProfile::Balanced) {
                qualityManager_.setProfile(transport::qos::QualityProfile::Quality);
                showToast("Profile set to Quality (1080p60, 15 Mbps)");
            } else if (curProf == transport::qos::QualityProfile::Quality) {
                qualityManager_.setProfile(transport::qos::QualityProfile::Adaptive);
                showToast("Profile set to Adaptive (Dynamic 2-20 Mbps)");
            } else {
                qualityManager_.setProfile(transport::qos::QualityProfile::Performance);
                showToast("Profile set to Performance (720p60, 3.5 Mbps)");
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Switch Quality Profile (Performance -> Balanced -> Quality -> Adaptive)");
        }

        ImGui::SameLine(0, 8.0f);

        // Audio & Input badges
        renderChip("Audio 48k", 1);
        ImGui::SameLine(0, 8.0f);
        renderChip((hostAllowRemoteInput_ || connectSendInput_) ? "Input On" : "Input Off",
                   (hostAllowRemoteInput_ || connectSendInput_) ? 1 : 0);
    } else {
        if (fontSmall_) ImGui::PushFont(fontSmall_);
        ImGui::TextColored(c.mute, "%.1f Mbps * %d ms", networkBitrate_, networkRtt_);
        if (fontSmall_) ImGui::PopFont();
    }

    ImGui::SameLine(hudW - (showLiveStreamingHud_ ? 210.0f : 100.0f));

    // Collapse / Expand Toggle
    if (ImGui::Button(showLiveStreamingHud_ ? "##CollapseHud" : "##ExpandHud", ImVec2(36.0f, 36.0f))) {
        showLiveStreamingHud_ = !showLiveStreamingHud_;
    }
    renderVectorIcon(showLiveStreamingHud_ ? "expand_less" : "expand_more",
                     ImVec2(ImGui::GetItemRectMin().x + 18.0f, ImGui::GetItemRectMin().y + 18.0f), 18.0f,
                     ImGui::ColorConvertFloat4ToU32(c.ink));

    if (showLiveStreamingHud_) {
        ImGui::SameLine(0, 8.0f);
        // Fullscreen Toggle
        if (ImGui::Button("##ToggleFs", ImVec2(36.0f, 36.0f))) {
            isStreamingFullscreen_ = !isStreamingFullscreen_;
            showToast(isStreamingFullscreen_ ? "Entered Fullscreen mode" : "Exited Fullscreen mode");
        }
        renderVectorIcon(isStreamingFullscreen_ ? "fullscreen_exit" : "fullscreen",
                         ImVec2(ImGui::GetItemRectMin().x + 18.0f, ImGui::GetItemRectMin().y + 18.0f), 18.0f,
                         ImGui::ColorConvertFloat4ToU32(c.ink));

        ImGui::SameLine(0, 8.0f);
        // Disconnect Button
        if (renderPillButton("Leave", false, true, ImVec2(80.0f, 36.0f), "call_end")) {
            terminateClientProcess();
            terminateHostProcess();
            isConnecting_ = false;
            connectStep_ = 0;
            navigateToScreen(DesktopScreen::Home);
            showToast("Stream ended");
        }
    }

    ImGui::EndGroup();
    ImGui::PopStyleVar();
}

void DesktopApplication::renderGuiLiveSessionScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 pStart = ImGui::GetCursorScreenPos();

    // 1. Dark viewport screen frame with subtle border
    ImVec2 vpMin = pStart;
    ImVec2 vpMax(pStart.x + avail.x, pStart.y + avail.y - 10.0f);

    drawList->AddRectFilled(vpMin, vpMax, IM_COL32(12, 18, 14, 255), tokens::MintShapes::Large);
    drawList->AddRect(vpMin, vpMax, ImGui::ColorConvertFloat4ToU32(c.line), tokens::MintShapes::Large, 0, 1.5f);

    // 2. Viewport content / center graphic
    ImVec2 center(vpMin.x + avail.x * 0.5f, vpMin.y + avail.y * 0.5f);

    if (isHostingActive_ || isConnecting_ || clientProcessPid_ != 0 || hostProcessPid_ != 0) {
        // Active stream simulation display
        drawList->AddCircleFilled(center, 48.0f, ImGui::ColorConvertFloat4ToU32(c.ct), 48.0f);
        renderVectorIcon("sports_esports", center, 44.0f, ImGui::ColorConvertFloat4ToU32(c.mint));

        if (fontTitle_) ImGui::PushFont(fontTitle_);
        const char* streamTitle = isHostingActive_ ? "Wi-Fi Host Broadcast Active" : "Remote Game Stream Running";
        ImVec2 ts = ImGui::CalcTextSize(streamTitle);
        drawList->AddText(ImVec2(center.x - ts.x * 0.5f, center.y + 60.0f), ImGui::ColorConvertFloat4ToU32(c.ink), streamTitle);
        if (fontTitle_) ImGui::PopFont();

        if (fontSmall_) ImGui::PushFont(fontSmall_);
        const char* sub = "Hardware NVENC 1080p60 * Direct Low-Latency WebRTC * Stereo Opus 48kHz";
        ImVec2 subTs = ImGui::CalcTextSize(sub);
        drawList->AddText(ImVec2(center.x - subTs.x * 0.5f, center.y + 90.0f), ImGui::ColorConvertFloat4ToU32(c.mute), sub);
        if (fontSmall_) ImGui::PopFont();
    } else {
        // Disconnected state
        renderVectorIcon("cast_connected", center, 44.0f, ImGui::ColorConvertFloat4ToU32(c.mute));

        if (fontTitle_) ImGui::PushFont(fontTitle_);
        const char* noTitle = "No Active Streaming Session";
        ImVec2 ts = ImGui::CalcTextSize(noTitle);
        drawList->AddText(ImVec2(center.x - ts.x * 0.5f, center.y + 50.0f), ImGui::ColorConvertFloat4ToU32(c.ink), noTitle);
        if (fontTitle_) ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(center.x - 110.0f, center.y + 90.0f));
        if (renderPillButton("Connect with Code", true, false, ImVec2(220.0f, 44.0f), "sports_esports")) {
            navigateToScreen(DesktopScreen::Connect);
        }
    }

    // 3. Render floating HUD over live session viewport
    renderStreamingHud();
}

void DesktopApplication::renderGuiLoadingScreen() {
    const auto& c = tokens::getActiveTheme(currentTheme_);
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    float dw = (dispSize.x > 100.0f) ? dispSize.x : static_cast<float>(guiWidth_);
    float dh = (dispSize.y > 100.0f) ? dispSize.y : static_cast<float>(guiHeight_);

    float cardW = std::min(480.0f, dw - 48.0f);
    float cardH = 360.0f;
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
        float logoSize = 60.0f;
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
        ImGui::BeginChild("RecoveryBox", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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
    float dlgW = std::min(540.0f, io.DisplaySize.x - 32.0f);
    float dlgH = 550.0f;
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

        // 6. Maximum Video Bitrate Slider (M3: strictly 2 to 25 Mbps)
        int prevBitrate = settingsMaxBitrateMbps_;
        if (renderM3Slider("MaxBitrateSlider", "Maximum video bitrate", &settingsMaxBitrateMbps_, 2, 25, "Mbps")) {
            if (settingsMaxBitrateMbps_ != prevBitrate) {
                auto cfg = qualityManager_.getProfileConfig();
                cfg.maxBitrateKbps = static_cast<uint32_t>(settingsMaxBitrateMbps_ * 1000);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 7. Frame Rate Select (M3 Segmented Button)
        if (fontBold_) ImGui::PushFont(fontBold_);
        ImGui::TextColored(c.ink, "Frame rate");
        if (fontBold_) ImGui::PopFont();
        ImGui::Spacing();
        if (renderM3SegmentedButton("FpsSeg", { "60 FPS (Ultra Smooth)", "30 FPS (Low Bandwidth)" }, &settingsFrameRate_)) {
            auto cfg = qualityManager_.getProfileConfig();
            cfg.targetFps = (settingsFrameRate_ == 0) ? 60 : 30;
            showToast(settingsFrameRate_ == 0 ? "Target set to 60 FPS" : "Target set to 30 FPS");
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Done Button
        float btnX = dlgW - 140.0f;
        ImGui::SetCursorPosX(btnX);
        if (renderPillButton("Done", true, false, ImVec2(110.0f, 42.0f))) {
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

        if (codeRemainingSeconds_ > 0) {
            codeTimer_ += dt;
            if (codeTimer_ >= 1.0f) {
                codeTimer_ = 0.0f;
                codeRemainingSeconds_--;
                if (codeRemainingSeconds_ == 0) {
                    auto codeRes = pairingManager_.generatePairingCode(300);
                    if (codeRes.hasValue()) {
                        pairingCode_ = codeRes.value();
                    } else {
                        pairingCode_ = generateRandomPairingCode();
                    }
                    codeRemainingSeconds_ = 300;
                    showToast("Pairing code refreshed: " + pairingCode_);
                }
            }
        }
    }

    if (isConnecting_) {
        connectStepTimer_ += dt;
        std::string sId(clientPairingCode_);
        if (sId == "fail" || sId == "error") {
            isConnecting_ = false;
            connectStep_ = 0;
            triggerError("Connection Handshake Failed",
                         "Failed to negotiate WebRTC offer/answer with pairing code '" + sId + "'.",
                         "1. Ensure the remote host is running and broadcasting.\n"
                         "2. Verify the 6-character pairing code is correct.\n"
                         "3. Check that your Wi-Fi network allows WebRTC peer packets.",
                         "ERR_RTC_HANDSHAKE_FAILED",
                         DesktopScreen::Connect);
        } else if (connectStepTimer_ > 0.6f && connectStep_ < 4) {
            connectStep_++;
            connectStepTimer_ = 0.0f;
        } else if (connectStep_ >= 4 && connectStepTimer_ > 0.6f) {
            connectStep_ = 5; // All done
            connectStatusMsg_ = "Connected. Hardware streaming active.";
            showToast("Connected to Wi-Fi stream!");
            isConnecting_ = false;
            navigateToScreen(DesktopScreen::LiveSession);
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

    checkBackgroundProcesses();

    // Real network telemetry sync from probe worker
    static float netTimer = 0.0f;
    netTimer += dt;
    if (netTimer >= 0.5f) {
        netTimer = 0.0f;
        std::lock_guard<std::mutex> lock(networkMutex_);
        float liveSpeed = liveNetworkStatus_.rxThroughputMbps + liveNetworkStatus_.txThroughputMbps;
        if (isHostingActive_) {
            networkBitrate_ = (liveSpeed > 1.0f) ? liveSpeed : hostBitrate_;
        } else {
            networkBitrate_ = (liveSpeed > 0.05f) ? liveSpeed : 0.4f;
        }

        networkRtt_ = (liveNetworkStatus_.gatewayPingMs > 0) ? liveNetworkStatus_.gatewayPingMs : 1;
        networkJitter_ = liveNetworkStatus_.jitterMs;
        networkLoss_ = liveNetworkStatus_.packetLossPercent;
        networkQosState_ = liveNetworkStatus_.qosState;

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

    // Ticking micro-interaction animations
    screenAlpha_ = std::min(1.0f, screenAlpha_ + dt * 8.0f);
    screenSlide_ = screenSlide_ * (1.0f - std::min(1.0f, dt * 10.0f));
    codePulseEffect_ = (std::sin(animTime * 3.5f) + 1.0f) * 0.5f;

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

    // Transparent Child for MainContent with smooth fade & slide transitions
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, screenAlpha_);
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + screenSlide_);

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
        case DesktopScreen::LiveSession:
            renderGuiLiveSessionScreen();
            break;
        case DesktopScreen::Error:
            renderGuiErrorScreen();
            break;
        default:
            renderGuiHomeScreen();
            break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();

    // 4. Overlays: Settings Dialog & Toast
    renderGuiSettingsDialog();
    renderGuiToast();
}

#endif // _WIN32

} // namespace mist::desktop
