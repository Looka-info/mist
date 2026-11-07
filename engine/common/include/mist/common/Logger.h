#pragma once

#include "Subsystem.h"
#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <functional>
#include <cstdint>

namespace mist::common {

enum class LogLevel : uint8_t {
    Debug = 0,
    Info,
    Warning,
    Error,
    Critical,
    Off
};

[[nodiscard]] constexpr std::string_view logLevelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO ";
        case LogLevel::Warning:  return "WARN ";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRIT ";
        case LogLevel::Off:      return "OFF  ";
    }
    return "UNKNOWN";
}

[[nodiscard]] LogLevel stringToLogLevel(std::string_view str) noexcept;

struct LogRecord {
    LogLevel level{LogLevel::Info};
    SubsystemId subsystem{SubsystemId::Common};
    std::string message;
    uint64_t timestampMs{0};
    uint32_t threadId{0};
};

using LogSinkCallback = std::function<void(const LogRecord& record, std::string_view formatted)>;

class Logger {
public:
    static Logger& instance() noexcept;

    void setLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel getLevel() const noexcept;

    void setConsoleEnabled(bool enabled) noexcept;
    void setLogFile(const std::string& path);
    void setCustomSink(LogSinkCallback callback);
    void resetSinks() noexcept;

    void log(LogLevel level, SubsystemId subsystem, std::string_view message);

    void debug(SubsystemId subsystem, std::string_view message) {
        log(LogLevel::Debug, subsystem, message);
    }
    void info(SubsystemId subsystem, std::string_view message) {
        log(LogLevel::Info, subsystem, message);
    }
    void warn(SubsystemId subsystem, std::string_view message) {
        log(LogLevel::Warning, subsystem, message);
    }
    void error(SubsystemId subsystem, std::string_view message) {
        log(LogLevel::Error, subsystem, message);
    }
    void critical(SubsystemId subsystem, std::string_view message) {
        log(LogLevel::Critical, subsystem, message);
    }

    // Explicit security boundary: verifies no obvious credentials or bearer tokens are contained
    [[nodiscard]] static bool containsSensitivePattern(std::string_view text) noexcept;

private:
    Logger() noexcept = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    mutable std::mutex mutex_;
    LogLevel currentLevel_{LogLevel::Info};
    bool consoleEnabled_{true};
    std::string logFilePath_;
    LogSinkCallback customSink_;
};

// Convenience logging macros for concise call sites
#define MIST_LOG_DEBUG(subsystem, msg) ::mist::common::Logger::instance().debug(subsystem, msg)
#define MIST_LOG_INFO(subsystem, msg)  ::mist::common::Logger::instance().info(subsystem, msg)
#define MIST_LOG_WARN(subsystem, msg)  ::mist::common::Logger::instance().warn(subsystem, msg)
#define MIST_LOG_ERROR(subsystem, msg) ::mist::common::Logger::instance().error(subsystem, msg)
#define MIST_LOG_CRIT(subsystem, msg)  ::mist::common::Logger::instance().critical(subsystem, msg)

} // namespace mist::common
