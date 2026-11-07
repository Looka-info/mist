#include "DesktopApplication.h"
#include "mist/common/Logger.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <iostream>

namespace mist::desktop {

DesktopApplication::DesktopApplication() noexcept = default;

DesktopApplication::~DesktopApplication() {
    if (isInitialized_.load()) {
        (void)shutdown();
    }
}

common::Result<std::optional<int>> DesktopApplication::initialize(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Mist Desktop v0.1.0\n"
                      << "Usage: mist_desktop [options]\n\n"
                      << "Options:\n"
                      << "  -h, --help     Show help information\n"
                      << "  -v, --version  Show version string\n";
            return std::optional<int>(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mist_desktop version 0.1.0\n";
            return std::optional<int>(0);
        }
    }

    auto valRes = config_.validate();
    if (!valRes) {
        return valRes.error();
    }

    MIST_LOG_INFO(common::SubsystemId::DesktopApp, "Initializing Mist Desktop Application v0.1.0...");
    (void)platform::windows::initializeProcessDpiAwareness();

    isInitialized_.store(true);
    return std::optional<int>(std::nullopt);
}

int DesktopApplication::run() {
    if (!isInitialized_.load()) {
        return 1;
    }
    MIST_LOG_INFO(common::SubsystemId::DesktopApp, "Mist Desktop entry point ready.");
    return 0;
}

common::Result<void> DesktopApplication::shutdown() {
    if (!isInitialized_.exchange(false)) {
        return common::Result<void>::success();
    }
    MIST_LOG_INFO(common::SubsystemId::DesktopApp, "Shutting down Mist Desktop...");
    return common::Result<void>::success();
}

} // namespace mist::desktop
