#pragma once

#include "mist/common/Config.h"
#include "mist/common/Result.h"
#include "mist/security/PairingManager.h"
#include "mist/transport/qos/QualityAdaptationManager.h"
#include <atomic>
#include <string>
#include <optional>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include "MintDesignTokens.h"
#endif

namespace mist::desktop {

enum class DesktopScreen : uint8_t {
    Home = 1,
    Host = 2,
    Connect = 3,
    Devices = 4,
    Network = 5,
    Error = 6,
    Pairing = 4,          // backward-compatible alias for Devices
    StreamSettings = 5,   // backward-compatible alias for Network
    LiveSession = 5,      // backward-compatible alias for Network
    DeviceManagement = 4, // backward-compatible alias for Devices
    Exit = 99
};

struct TrustedDeviceItem {
    std::string name;
    std::string lastSeen;
    bool allowKbdMouse{true};
    bool allowController{true};
};

struct NearbyWifiNetwork {
    std::string ssid;
    int signalPercent{0};
    int rssiDbm{-100};
    float frequencyGhz{0.0f};
    int channel{0};
    std::string standard;
    std::string security;
};

struct LiveNetworkStatus {
    std::string adapterName{"Wired Ethernet"};
    std::string connectionType{"Wired Ethernet (802.3)"};
    bool isWifi{false};
    bool isConnected{true};
    std::string ipv4Address{"192.168.1.8"};
    std::string gatewayAddress{"192.168.1.1"};
    std::string macAddress;
    uint64_t linkSpeedMbps{100};

    // Real Wi-Fi probe details
    std::string wifiSsid;
    std::string wifiBssid;
    int wifiSignalQualityPercent{0};
    int wifiRssiDbm{-100};
    float wifiFrequencyGhz{0.0f};
    int wifiChannel{0};
    std::string wifiStandard;
    std::string wifiAuthCipher;

    // Live Real-Time Throughput (Rx / Tx)
    float rxThroughputMbps{0.0f};
    float txThroughputMbps{0.0f};

    // Live Real-Time Latency & Jitter
    int gatewayPingMs{0};
    int internetPingMs{0};
    int jitterMs{0};
    float packetLossPercent{0.0f};
    std::string qosState{"Stable"};

    std::vector<NearbyWifiNetwork> nearbyWifiList;
};

struct LiveSystemHardware {
    std::string gpuName{"NVIDIA GeForce GTX 1660 SUPER"};
    std::string gpuDriver{"617.14"};
    bool hasNvenc{true};
    std::string displayInfo{"1920x1080 @ 60Hz"};
    std::string audioEndpoint{"Default Audio Device"};
    std::string gamepadStatus{"No controller detected"};
    bool hasGamepad{false};
};

class DesktopApplication {
public:
    DesktopApplication() noexcept;
    ~DesktopApplication();

    [[nodiscard]] common::Result<std::optional<int>> initialize(int argc, char* argv[]);
    [[nodiscard]] int run();
    [[nodiscard]] common::Result<void> shutdown();

    [[nodiscard]] const common::AppConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] security::PairingManager& getPairingManager() noexcept { return pairingManager_; }
    [[nodiscard]] transport::qos::QualityAdaptationManager& getQualityManager() noexcept { return qualityManager_; }

    // Screen Renderers (Console & Non-interactive Mode)
    void renderHomeScreen() const;
    void renderPairingScreen();
    void renderHostScreen() const;
    void renderSettingsScreen() const;
    void renderLiveSessionScreen() const;
    void renderDeviceManagementScreen() const;
    void renderConnectScreen() const;
    void renderNetworkScreen() const;

#if defined(_WIN32)
    // Direct3D 11 Win32 GUI
    [[nodiscard]] common::Result<void> runGuiLoop();
    void handleResize(uint32_t width, uint32_t height);
#endif

private:
    void printHeader(const std::string& title) const;

#if defined(_WIN32)
    [[nodiscard]] common::Result<void> initializeD3D11Gui();
    void cleanupD3D11Gui();
    void applyMintAndWhiteTheme();
    void applyActiveTheme();
    void toggleTheme();
    void renderGuiFrame();

    // GUI Screen Components (Matching Mist.html design specification)
    void renderGuiHeader();
    void renderGuiNavRail();
    void renderGuiHomeScreen();
    void renderGuiHostScreen();
    void renderGuiConnectScreen();
    void renderGuiDevicesScreen();
    void renderGuiNetworkScreen();
    void renderGuiLoadingScreen();
    void renderGuiErrorScreen();
    void renderGuiSettingsDialog();
    void renderGuiToast();

    // GUI Visual Components
    void renderSleekMintLogo(float size = 36.0f) const;
    void renderVectorIcon(const char* name, const ImVec2& center, float size, ImU32 color) const;
    void renderLoadingSpinner(const ImVec2& center, float radius, ImU32 color, float thickness = 3.0f) const;
    void renderChip(const char* text, int statusType = 0) const; // 0=neutral, 1=ok, 2=warn, 3=bad
    void renderMetricBox(const char* label, const char* value, const char* sublabel = nullptr) const;
    bool renderPillButton(const char* label, bool primary = false, bool danger = false, const ImVec2& size = ImVec2(0, 40.0f), const char* iconName = nullptr);
    
    // Material Design 3 Native Components (m3.material.io)
    bool renderM3Switch(const char* id, const char* label, bool* v, const char* sublabel = nullptr, float width = 0.0f);
    bool renderM3TextField(const char* id, const char* label, char* buf, size_t bufSize, const char* hint = nullptr, float width = 0.0f);
    bool renderM3SegmentedButton(const char* id, const std::vector<const char*>& options, int* selectedIndex, float width = 0.0f);
    bool renderM3Slider(const char* id, const char* label, int* v, int vMin, int vMax, const char* unit = "Mbps");

    void showToast(const std::string& message);
    void triggerError(const std::string& title, const std::string& message,
                      const std::string& suggestion = "", const std::string& code = "ERR_GENERIC",
                      DesktopScreen returnScreen = DesktopScreen::Home);
    void clearError();
    void generateOrganicShapes();

    struct ErrorInfo {
        bool hasError{false};
        std::string title;
        std::string code;
        std::string message;
        std::string suggestion;
        DesktopScreen returnScreen{DesktopScreen::Home};
    };
    ErrorInfo activeError_;

    // Initial Loading Splash State
    bool isInitialLoading_{true};
    float initialLoadingTimer_{0.0f};
    float initialLoadingDuration_{1.2f};
    std::string initialLoadingStage_{"Initializing Direct3D 11 & Hardware NVENC..."};

    struct OrganicShape {
        int templateType{0};
        int symmetry{8};
        float param{0.35f};
        float phase{0.0f};
        float scale{300.0f};
        float posXRatio{0.5f};
        float posYRatio{0.5f};
        float driftX{40.0f};
        float driftY{40.0f};
        float driftDuration{30.0f};
        int colorIndex{0}; // 0..3 for s1..s4
    };
    std::vector<OrganicShape> organicShapes_;

    HWND hwnd_{nullptr};
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> logoTextureSrv_;

    uint32_t guiWidth_{1280};
    uint32_t guiHeight_{800};
    bool isNavRailExpanded_{true};
    DesktopScreen currentScreen_{DesktopScreen::Home};
    tokens::ThemeMode currentTheme_{tokens::ThemeMode::Light};

    // Typography Handles (Segoe UI TrueType matching design scale)
    ImFont* fontRegular_{nullptr};
    ImFont* fontBold_{nullptr};
    ImFont* fontTitle_{nullptr};
    ImFont* fontHeadline_{nullptr};
    ImFont* fontDisplay_{nullptr};
    ImFont* fontSmall_{nullptr};
    ImFont* fontCode_{nullptr};
    ImFont* fontIcons_{nullptr};
    ImFont* fontIconsLarge_{nullptr};

    // GUI Interactive State (Matching Mist.html)
    bool isSettingsOpen_{false};
    char searchQuery_[128]{""};
    std::string toastMessage_;
    float toastTimer_{0.0f};

    // Host View State
    bool isHostingActive_{false};
    float hostFps_{60.0f};
    float hostBitrate_{11.5f};
    float hostEncodeMs_{1.6f};
    bool hostAudioStream_{true};
    bool hostAllowRemoteInput_{false};
    char hostSessionId_[64]{"mist-live"};
    char hostSignalingServer_[64]{"ws://127.0.0.1:8080"};

    // Connect View State
    char connectSessionId_[64]{"mist-live"};
    char connectSignalingServer_[64]{"ws://127.0.0.1:8080"};
    char connectRelayServer_[64]{""};
    bool connectHearAudio_{true};
    bool connectSendInput_{true};
    bool isConnecting_{false};
    int connectStep_{0};
    float connectStepTimer_{0.0f};
    std::string connectStatusMsg_{"Not connected."};

    // Devices View State
    std::string pairingCode_{"7DK4LM"};
    int wrongCodeFails_{0};
    int codeLockSeconds_{0};
    float codeLockTimer_{0.0f};
    std::string codeHintMsg_{"Read this code to the person pairing. It works once."};
    bool isDeviceApprovalPending_{true};
    std::vector<TrustedDeviceItem> trustedDevices_;

    // Network View State
    float networkBitrate_{12.0f};
    int networkRtt_{18};
    float networkLoss_{0.0f};
    int networkJitter_{2};
    std::string networkQosState_{"Stable"};
    std::vector<float> bitrateHistory_;
    std::vector<float> rttHistory_;
    bool isNetworkCongested_{false};
    float congestionDuration_{0.0f};

    // Settings Modal State
    int settingsTheme_{0}; // 0 = Light, 1 = Dark
    int settingsContrast_{0}; // 0 = Standard, 1 = High
    bool settingsCompactView_{false};
    bool settingsAnimatedShapes_{true};
    int settingsMaxBitrateMbps_{15};
    int settingsFrameRate_{0}; // 0 = 60 fps, 1 = 30 fps

    // Legacy/compatibility members
    bool allowAudioLoopback_{true};
    bool allowKeyboard_{true};
    bool allowMouse_{true};
    bool allowGamepad_{true};
    bool localOperatorPriority_{true};
    int selectedProfileIndex_{3};
    int targetBitrateKbps_{8000};
    int minBitrateKbps_{2000};
    int maxBitrateKbps_{20000};
    bool vsyncEnabled_{false};
    char manualPairingCodeInput_[32]{""};
    char quickConnectHostInput_[64]{"127.0.0.1:8080"};
    bool sampleRequestPending_{true};
    int selectedMonitorIndex_{0};
    // Real System & Network Monitor Worker
    void startNetworkMonitor();
    void stopNetworkMonitor();
    void networkMonitorWorker();
    void probeLiveHardware();
    void probeLiveNetwork();

    std::thread networkThread_;
    std::atomic<bool> networkThreadRunning_{false};
    std::mutex networkMutex_;
    LiveNetworkStatus liveNetworkStatus_;
    LiveSystemHardware liveSystemHardware_;
    uint64_t prevInOctets_{0};
    uint64_t prevOutOctets_{0};
    std::chrono::steady_clock::time_point prevSampleTime_{std::chrono::steady_clock::now()};
    int prevGatewayPing_{-1};

    // Real Process Spawning for Streaming
    HANDLE hHostProcess_{nullptr};
    DWORD hostProcessPid_{0};
    HANDLE hClientProcess_{nullptr};
    DWORD clientProcessPid_{0};

    bool launchHostProcess(const std::string& sessionId, const std::string& signalingUrl, bool enableAudio, bool allowInput);
    void terminateHostProcess();
    bool launchClientProcess(const std::string& sessionId, const std::string& signalingUrl, bool hearAudio, bool sendInput);
    void terminateClientProcess();
    void checkBackgroundProcesses();

    bool emergencyDisconnectTriggered_{false};
#endif

    common::AppConfig config_;
    security::PairingManager pairingManager_;
    transport::qos::QualityAdaptationManager qualityManager_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};

    bool launchInteractive_{false};
    std::optional<DesktopScreen> targetScreen_;
    bool runSelfTest_{false};
    bool headlessMode_{false};
};

} // namespace mist::desktop
