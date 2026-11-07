#pragma once

#include "mist/common/Config.h"
#include "mist/common/Result.h"
#include <atomic>
#include <string>
#include <optional>

namespace mist::host {

enum class HostCommandMode {
    Normal,
    Help,
    Version,
    SelfTest,
    ListDisplays,
    CaptureTest,
    EncodeTest,
    CaptureEncodeTest
};

struct HostCommandLineOptions {
    HostCommandMode mode{HostCommandMode::Normal};
    std::string configPath;
    uint32_t displayIndex{0};
    uint32_t durationSeconds{30};
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t fps{60};
    std::string codec{"h264"};
    std::string outputPath;
    std::string reportPath;
};

class HostApplication {
public:
    HostApplication() noexcept;
    ~HostApplication();

    [[nodiscard]] common::Result<std::optional<int>> initialize(int argc, char* argv[]);
    [[nodiscard]] int run();
    void requestShutdown() noexcept;
    [[nodiscard]] common::Result<void> shutdown();

    [[nodiscard]] const common::AppConfig& getConfig() const noexcept { return config_; }

private:
    [[nodiscard]] common::Result<void> runSelfTest();
    [[nodiscard]] int executeListDisplays();
    [[nodiscard]] int executeCaptureTest();
    [[nodiscard]] int executeEncodeTest();
    [[nodiscard]] int executeCaptureEncodeTest();

    common::AppConfig config_;
    HostCommandLineOptions options_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};
};

} // namespace mist::host
