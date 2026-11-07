#pragma once

#include "Result.h"
#include "Logger.h"
#include <string>
#include <string_view>
#include <cstdint>

namespace mist::common {

constexpr uint32_t CURRENT_CONFIG_SCHEMA_VERSION = 1;

struct LoggingConfig {
    LogLevel level{LogLevel::Info};
    bool logToFile{false};
    std::string filePath{"mist.log"};

    [[nodiscard]] Result<void> validate() const noexcept;
};

struct HostConfig {
    std::string bindAddress{"0.0.0.0"};
    uint16_t port{47998};
    uint32_t targetFps{60};
    uint32_t targetBitrateKbps{15000};
    bool enableHardwareEncoding{true};
    uint32_t maxSessions{1};
    bool allowRemoteInput{true};

    [[nodiscard]] Result<void> validate() const noexcept;
};

struct ClientConfig {
    std::string serverAddress{"127.0.0.1"};
    uint16_t serverPort{47998};
    bool lowLatencyMode{true};
    bool fullscreen{false};
    bool statsOverlay{false};

    [[nodiscard]] Result<void> validate() const noexcept;
};

class AppConfig {
public:
    uint32_t schemaVersion{CURRENT_CONFIG_SCHEMA_VERSION};
    std::string appName{"Mist"};
    LoggingConfig logging;
    HostConfig host;
    ClientConfig client;

    [[nodiscard]] Result<void> validate() const noexcept;

    [[nodiscard]] std::string serializeToString() const;
    static Result<AppConfig> loadFromString(std::string_view content);
    static Result<AppConfig> loadFromFile(const std::string& path);
    Result<void> saveToFile(const std::string& path) const;
};

} // namespace mist::common
