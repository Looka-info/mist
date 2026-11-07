#include "HostApplication.h"
#include "mist/common/Logger.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace mist::host {

HostApplication::HostApplication() noexcept = default;

HostApplication::~HostApplication() {
    if (isInitialized_.load()) {
        (void)shutdown();
    }
}

common::Result<std::optional<int>> HostApplication::initialize(int argc, char* argv[]) {
    bool selfTestRequested = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Mist Host v0.1.0\n"
                      << "Usage: mist_host [options]\n\n"
                      << "Options:\n"
                      << "  -h, --help       Show help information\n"
                      << "  -v, --version    Show version string\n"
                      << "  --self-test      Run internal self-tests and exit\n"
                      << "  --config <path>  Load custom configuration file\n";
            return std::optional<int>(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mist_host version 0.1.0\n";
            return std::optional<int>(0);
        } else if (arg == "--self-test") {
            selfTestRequested = true;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                configPath_ = argv[++i];
            } else {
                return common::Error(common::SubsystemId::HostApp, common::ErrorCode::InvalidArgument, "Missing path argument for --config");
            }
        } else {
            return common::Error(common::SubsystemId::HostApp, common::ErrorCode::InvalidArgument, "Unknown command line option: " + std::string(arg));
        }
    }

    // Load configuration
    if (!configPath_.empty()) {
        auto loaded = common::AppConfig::loadFromFile(configPath_);
        if (!loaded) {
            return loaded.error();
        }
        config_ = loaded.value();
    } else {
        auto validated = config_.validate();
        if (!validated) {
            return validated.error();
        }
    }

    // Initialize logging according to configuration
    common::Logger::instance().setLevel(config_.logging.level);
    if (config_.logging.logToFile) {
        common::Logger::instance().setLogFile(config_.logging.filePath);
    }

    MIST_LOG_INFO(common::SubsystemId::HostApp, "Initializing Mist Host Application v0.1.0...");

    // Initialize platform settings (e.g. DPI awareness)
    if (auto dpiRes = platform::windows::initializeProcessDpiAwareness(); !dpiRes) {
        MIST_LOG_WARN(common::SubsystemId::HostApp, "Failed to initialize DPI awareness");
    }

    isInitialized_.store(true);

    if (selfTestRequested) {
        MIST_LOG_INFO(common::SubsystemId::HostApp, "Running Host self-test suite...");
        auto testRes = runSelfTest();
        if (!testRes) {
            MIST_LOG_ERROR(common::SubsystemId::HostApp, "Self-test failed: " + testRes.error().format());
            return std::optional<int>(1);
        }
        MIST_LOG_INFO(common::SubsystemId::HostApp, "Self-test passed successfully.");
        return std::optional<int>(0);
    }

    return std::optional<int>(std::nullopt);
}

common::Result<void> HostApplication::runSelfTest() {
    // 1. Verify configuration integrity
    if (auto res = config_.host.validate(); !res) {
        return res;
    }

    // 2. Verify high-resolution platform timer
    platform::windows::HighResolutionTimer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (timer.elapsedMicroseconds() <= 0.0) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError, "HighResolutionTimer query failed");
    }

    // 3. Verify logging system
    MIST_LOG_DEBUG(common::SubsystemId::HostApp, "Self-test debug log verification");

    return common::Result<void>::success();
}

int HostApplication::run() {
    if (!isInitialized_.load()) {
        std::cerr << "Cannot run Host application: not initialized." << std::endl;
        return 1;
    }

    isRunning_.store(true);
    MIST_LOG_INFO(common::SubsystemId::HostApp, "Mist Host is running. Awaiting connections... (Milestone 0: Idle state)");

    // In non-interactive or testing mode, ensure we can be safely stopped
    while (isRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // shortcut: Milestone 0 does not implement streaming loop, stops immediately if not interactive
        break;
    }

    MIST_LOG_INFO(common::SubsystemId::HostApp, "Mist Host run cycle finished.");
    return 0;
}

void HostApplication::requestShutdown() noexcept {
    isRunning_.store(false);
}

common::Result<void> HostApplication::shutdown() {
    if (!isInitialized_.exchange(false)) {
        return common::Result<void>::success();
    }

    MIST_LOG_INFO(common::SubsystemId::HostApp, "Shutting down Mist Host Application...");
    isRunning_.store(false);
    return common::Result<void>::success();
}

} // namespace mist::host
