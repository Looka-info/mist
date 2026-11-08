#pragma once

#include "mist/common/Config.h"
#include "mist/common/Result.h"
#include "mist/security/PairingManager.h"
#include "mist/transport/qos/QualityAdaptationManager.h"
#include <atomic>
#include <string>
#include <optional>

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

    // Screen Renderers
    void renderHomeScreen() const;
    void renderPairingScreen();
    void renderHostScreen() const;
    void renderSettingsScreen() const;
    void renderLiveSessionScreen() const;
    void renderDeviceManagementScreen() const;

private:
    void printHeader(const std::string& title) const;

    common::AppConfig config_;
    security::PairingManager pairingManager_;
    transport::qos::QualityAdaptationManager qualityManager_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};

    bool launchInteractive_{false};
    std::optional<DesktopScreen> targetScreen_;
    bool runSelfTest_{false};
};

} // namespace mist::desktop
