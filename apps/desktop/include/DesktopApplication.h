#pragma once

#include "mist/common/Config.h"
#include "mist/common/Result.h"
#include <atomic>
#include <string>

namespace mist::desktop {

class DesktopApplication {
public:
    DesktopApplication() noexcept;
    ~DesktopApplication();

    [[nodiscard]] common::Result<std::optional<int>> initialize(int argc, char* argv[]);
    [[nodiscard]] int run();
    [[nodiscard]] common::Result<void> shutdown();

    [[nodiscard]] const common::AppConfig& getConfig() const noexcept { return config_; }

private:
    common::AppConfig config_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};
};

} // namespace mist::desktop
