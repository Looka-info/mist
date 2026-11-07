#pragma once

#include "mist/common/Config.h"
#include "mist/common/Result.h"
#include <atomic>
#include <string>

namespace mist::host {

class HostApplication {
public:
    HostApplication() noexcept;
    ~HostApplication();

    // Returns an exit code directly if an immediate exit was requested (e.g. --help, --version, --self-test),
    // or std::nullopt to indicate the application should proceed to run().
    [[nodiscard]] common::Result<std::optional<int>> initialize(int argc, char* argv[]);

    [[nodiscard]] int run();
    void requestShutdown() noexcept;
    [[nodiscard]] common::Result<void> shutdown();

    [[nodiscard]] const common::AppConfig& getConfig() const noexcept { return config_; }

private:
    [[nodiscard]] common::Result<void> runSelfTest();

    common::AppConfig config_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};
    std::string configPath_;
};

} // namespace mist::host
