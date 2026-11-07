#pragma once

#include "mist/common/Config.h"
#include "mist/common/Result.h"
#include "mist/decode/DecodePresentPipeline.h"
#include <atomic>
#include <string>
#include <optional>

namespace mist::client {

enum class ClientCommandMode {
    Normal,
    DecoderInfo,
    DecodeTest,
    PresentTest,
    DecodeBenchmark,
    SelfTest
};

struct ClientCliOptions {
    ClientCommandMode mode{ClientCommandMode::Normal};
    std::string configPath;
    std::string inputPath;
    std::string reportPath;
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t fps{60};
    uint32_t durationSeconds{5};
    bool headless{false};
    bool vsync{false};
};

class ClientApplication {
public:
    ClientApplication() noexcept;
    ~ClientApplication();

    [[nodiscard]] common::Result<std::optional<int>> initialize(int argc, char* argv[]);
    [[nodiscard]] int run();
    void requestShutdown() noexcept;
    [[nodiscard]] common::Result<void> shutdown();

    [[nodiscard]] const common::AppConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] const ClientCliOptions& getCliOptions() const noexcept { return options_; }

private:
    [[nodiscard]] int executeDecoderInfo();
    [[nodiscard]] int executeDecodeTest();
    [[nodiscard]] int executePresentTest();
    [[nodiscard]] int executeDecodeBenchmark();
    [[nodiscard]] common::Result<void> runSelfTest();

    common::AppConfig config_;
    ClientCliOptions options_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};
};

} // namespace mist::client
