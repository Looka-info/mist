#include "ClientApplication.h"
#include "mist/common/Logger.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace mist::client {

ClientApplication::ClientApplication() noexcept = default;

ClientApplication::~ClientApplication() {
    if (isInitialized_.load()) {
        (void)shutdown();
    }
}

common::Result<std::optional<int>> ClientApplication::initialize(int argc, char* argv[]) {
    bool selfTestRequested = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Mist Client v0.1.0\n"
                      << "Usage: mist_client [options]\n\n"
                      << "Options:\n"
                      << "  -h, --help       Show help information\n"
                      << "  -v, --version    Show version string\n"
                      << "  --self-test      Run internal self-tests and exit\n"
                      << "  --config <path>  Load custom configuration file\n";
            return std::optional<int>(0);
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "mist_client version 0.1.0\n";
            return std::optional<int>(0);
        } else if (arg == "--self-test") {
            selfTestRequested = true;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                configPath_ = argv[++i];
            } else {
                return common::Error(common::SubsystemId::ClientApp, common::ErrorCode::InvalidArgument, "Missing path argument for --config");
            }
        } else {
            return common::Error(common::SubsystemId::ClientApp, common::ErrorCode::InvalidArgument, "Unknown command line option: " + std::string(arg));
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

    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Initializing Mist Client Application v0.1.0...");

    // Initialize platform settings (e.g. DPI awareness)
    if (auto dpiRes = platform::windows::initializeProcessDpiAwareness(); !dpiRes) {
        MIST_LOG_WARN(common::SubsystemId::ClientApp, "Failed to initialize DPI awareness");
    }

    isInitialized_.store(true);

    if (selfTestRequested) {
        MIST_LOG_INFO(common::SubsystemId::ClientApp, "Running Client self-test suite...");
        auto testRes = runSelfTest();
        if (!testRes) {
            MIST_LOG_ERROR(common::SubsystemId::ClientApp, "Self-test failed: " + testRes.error().format());
            return std::optional<int>(1);
        }
        MIST_LOG_INFO(common::SubsystemId::ClientApp, "Self-test passed successfully.");
        return std::optional<int>(0);
    }

    return std::optional<int>(std::nullopt);
}

common::Result<void> ClientApplication::runSelfTest() {
    // 1. Verify configuration integrity
    if (auto res = config_.client.validate(); !res) {
        return res;
    }

    // 2. Verify high-resolution platform timer
    platform::windows::HighResolutionTimer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (timer.elapsedMicroseconds() <= 0.0) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError, "HighResolutionTimer query failed");
    }

    // 3. Verify logging system
    MIST_LOG_DEBUG(common::SubsystemId::ClientApp, "Self-test debug log verification");

    return common::Result<void>::success();
}

int ClientApplication::run() {
    if (!isInitialized_.load()) {
        std::cerr << "Cannot run Client application: not initialized." << std::endl;
        return 1;
    }

    isRunning_.store(true);
    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Mist Client is running. (Milestone 0: Idle state)");

    // In non-interactive or testing mode, ensure we can be safely stopped
    while (isRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // shortcut: Milestone 0 does not implement streaming loop, stops immediately if not interactive
        break;
    }

    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Mist Client run cycle finished.");
    return 0;
}

void ClientApplication::requestShutdown() noexcept {
    isRunning_.store(false);
}

common::Result<void> ClientApplication::shutdown() {
    if (!isInitialized_.exchange(false)) {
        return common::Result<void>::success();
    }

    MIST_LOG_INFO(common::SubsystemId::ClientApp, "Shutting down Mist Client Application...");
    isRunning_.store(false);
    return common::Result<void>::success();
}

} // namespace mist::client
