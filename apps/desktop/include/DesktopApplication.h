#pragma once

#include "mist/common/Config.h"
#include "mist/common/Result.h"
#include "mist/security/PairingManager.h"
#include "mist/transport/qos/QualityAdaptationManager.h"
#include <atomic>
#include <string>
#include <optional>
#include <vector>

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
#endif

namespace mist::desktop {

enum class DesktopScreen : uint8_t {
    Home = 1,
    Pairing,
    Host,
    StreamSettings,
    LiveSession,
    DeviceManagement,
    Exit
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
    void renderGuiFrame();

    // GUI Screen Components
    void renderGuiHeader();
    void renderGuiNavRail();
    void renderGuiHomeScreen();
    void renderGuiPairingScreen();
    void renderGuiHostScreen();
    void renderGuiSettingsScreen();
    void renderGuiLiveSessionScreen();
    void renderGuiDeviceManagementScreen();

    // GUI Visual Components
    void renderBadge(const char* text, bool isSuccess = true) const;
    void renderMetricBox(const char* label, const char* value, const char* sublabel = nullptr) const;

    HWND hwnd_{nullptr};
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;

    uint32_t guiWidth_{1280};
    uint32_t guiHeight_{800};
    bool isNavRailExpanded_{true};
    DesktopScreen currentScreen_{DesktopScreen::Home};

    // GUI Interactive State
    bool isHostingActive_{true};
    bool allowAudioLoopback_{true};
    bool allowKeyboard_{true};
    bool allowMouse_{true};
    bool allowGamepad_{true};
    bool localOperatorPriority_{true};

    int selectedProfileIndex_{3}; // 0: Performance, 1: Balanced, 2: Quality, 3: Adaptive
    int targetBitrateKbps_{8000};
    int minBitrateKbps_{2000};
    int maxBitrateKbps_{20000};
    bool vsyncEnabled_{false};

    char manualPairingCodeInput_[32]{""};
    bool sampleRequestPending_{true};
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
