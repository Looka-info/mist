#include "mist/common/Logger.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace mist::common {

LogLevel stringToLogLevel(std::string_view str) noexcept {
    std::string s(str);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (s == "debug") return LogLevel::Debug;
    if (s == "info") return LogLevel::Info;
    if (s == "warning" || s == "warn") return LogLevel::Warning;
    if (s == "error") return LogLevel::Error;
    if (s == "critical" || s == "crit") return LogLevel::Critical;
    if (s == "off") return LogLevel::Off;
    return LogLevel::Info;
}

Logger& Logger::instance() noexcept {
    static Logger instance;
    return instance;
}

void Logger::setLevel(LogLevel level) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

LogLevel Logger::getLevel() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentLevel_;
}

void Logger::setConsoleEnabled(bool enabled) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    consoleEnabled_ = enabled;
}

void Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    logFilePath_ = path;
}

void Logger::setCustomSink(LogSinkCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    customSink_ = std::move(callback);
}

void Logger::resetSinks() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    consoleEnabled_ = true;
    logFilePath_.clear();
    customSink_ = nullptr;
    currentLevel_ = LogLevel::Info;
}

bool Logger::containsSensitivePattern(std::string_view text) noexcept {
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("bearer ") != std::string::npos ||
           lower.find("password=") != std::string::npos ||
           lower.find("token=") != std::string::npos ||
           lower.find("private_key") != std::string::npos ||
           lower.find("secret=") != std::string::npos;
}

void Logger::log(LogLevel level, SubsystemId subsystem, std::string_view message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < currentLevel_ || currentLevel_ == LogLevel::Off) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const auto timeT = std::chrono::system_clock::to_time_t(now);

    std::tm timeInfo{};
#if defined(_WIN32)
    localtime_s(&timeInfo, &timeT);
    const uint32_t threadId = static_cast<uint32_t>(::GetCurrentThreadId());
#else
    localtime_r(&timeT, &timeInfo);
    const uint32_t threadId = 0;
#endif

    const auto ms = nowMs % 1000;

    std::ostringstream formatted;
    formatted << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << ms
              << " [" << std::setfill(' ') << std::setw(5) << logLevelToString(level) << "]"
              << " [T:" << std::setw(5) << threadId << "]"
              << " [" << std::setw(10) << subsystemToString(subsystem) << "] ";

    std::string sanitizedMessage(message);
    if (containsSensitivePattern(message)) {
        sanitizedMessage = "[REDACTED_SENSITIVE_DATA]";
    }
    formatted << sanitizedMessage;

    const std::string line = formatted.str();

    LogRecord record{
        .level = level,
        .subsystem = subsystem,
        .message = sanitizedMessage,
        .timestampMs = static_cast<uint64_t>(nowMs),
        .threadId = threadId
    };

    if (consoleEnabled_) {
        if (level >= LogLevel::Error) {
            std::cerr << line << std::endl;
        } else {
            std::cout << line << std::endl;
        }
    }

    if (!logFilePath_.empty()) {
        std::ofstream file(logFilePath_, std::ios::app);
        if (file.is_open()) {
            file << line << std::endl;
        }
    }

    if (customSink_) {
        customSink_(record, line);
    }
}

} // namespace mist::common
