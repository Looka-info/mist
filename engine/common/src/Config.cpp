#include "mist/common/Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace mist::common {

static std::string trim(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(start, end - start + 1));
}

Result<void> LoggingConfig::validate() const noexcept {
    if (logToFile && filePath.empty()) {
        return Error(SubsystemId::Common, ErrorCode::InvalidArgument, "Log file path cannot be empty when logToFile is enabled");
    }
    return Result<void>::success();
}

Result<void> HostConfig::validate() const noexcept {
    if (bindAddress.empty()) {
        return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Host bind address cannot be empty");
    }
    if (port == 0) {
        return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Host port cannot be 0");
    }
    if (targetFps < 15 || targetFps > 240) {
        return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Host targetFps must be between 15 and 240");
    }
    if (targetBitrateKbps < 500 || targetBitrateKbps > 100000) {
        return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Host targetBitrateKbps must be between 500 and 100000");
    }
    if (maxSessions == 0 || maxSessions > 8) {
        return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Host maxSessions must be between 1 and 8");
    }
    return Result<void>::success();
}

Result<void> ClientConfig::validate() const noexcept {
    if (serverAddress.empty()) {
        return Error(SubsystemId::ClientApp, ErrorCode::InvalidArgument, "Client server address cannot be empty");
    }
    if (serverPort == 0) {
        return Error(SubsystemId::ClientApp, ErrorCode::InvalidArgument, "Client server port cannot be 0");
    }
    return Result<void>::success();
}

Result<void> AppConfig::validate() const noexcept {
    if (schemaVersion == 0 || schemaVersion > CURRENT_CONFIG_SCHEMA_VERSION) {
        return Error(SubsystemId::Common, ErrorCode::FormatNotSupported, "Unsupported configuration schema version");
    }
    if (auto res = logging.validate(); !res) {
        return res.error();
    }
    if (auto res = host.validate(); !res) {
        return res.error();
    }
    if (auto res = client.validate(); !res) {
        return res.error();
    }
    return Result<void>::success();
}

std::string AppConfig::serializeToString() const {
    std::ostringstream oss;
    oss << "# Mist Configuration\n";
    oss << "schema_version=" << schemaVersion << "\n";
    oss << "app_name=" << appName << "\n\n";

    oss << "[logging]\n";
    oss << "level=" << logLevelToString(logging.level) << "\n";
    oss << "log_to_file=" << (logging.logToFile ? "true" : "false") << "\n";
    oss << "file_path=" << logging.filePath << "\n\n";

    oss << "[host]\n";
    oss << "bind_address=" << host.bindAddress << "\n";
    oss << "port=" << host.port << "\n";
    oss << "target_fps=" << host.targetFps << "\n";
    oss << "target_bitrate_kbps=" << host.targetBitrateKbps << "\n";
    oss << "enable_hw_encode=" << (host.enableHardwareEncoding ? "true" : "false") << "\n";
    oss << "max_sessions=" << host.maxSessions << "\n";
    oss << "allow_remote_input=" << (host.allowRemoteInput ? "true" : "false") << "\n\n";

    oss << "[client]\n";
    oss << "server_address=" << client.serverAddress << "\n";
    oss << "server_port=" << client.serverPort << "\n";
    oss << "low_latency_mode=" << (client.lowLatencyMode ? "true" : "false") << "\n";
    oss << "fullscreen=" << (client.fullscreen ? "true" : "false") << "\n";
    oss << "stats_overlay=" << (client.statsOverlay ? "true" : "false") << "\n";

    return oss.str();
}

Result<AppConfig> AppConfig::loadFromString(std::string_view content) {
    AppConfig config;
    std::string currentSection;
    std::istringstream stream((std::string(content)));
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentSection = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        auto eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, eqPos));
        std::string val = trim(trimmed.substr(eqPos + 1));

        if (currentSection.empty()) {
            if (key == "schema_version") {
                try {
                    config.schemaVersion = static_cast<uint32_t>(std::stoul(val));
                } catch (...) {
                    return Error(SubsystemId::Common, ErrorCode::InvalidArgument, "Invalid schema_version integer");
                }
            } else if (key == "app_name") {
                config.appName = val;
            }
        } else if (currentSection == "logging") {
            if (key == "level") {
                config.logging.level = stringToLogLevel(val);
            } else if (key == "log_to_file") {
                config.logging.logToFile = (val == "true" || val == "1");
            } else if (key == "file_path") {
                config.logging.filePath = val;
            }
        } else if (currentSection == "host") {
            if (key == "bind_address") {
                config.host.bindAddress = val;
            } else if (key == "port") {
                try {
                    config.host.port = static_cast<uint16_t>(std::stoul(val));
                } catch (...) {
                    return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Invalid host port integer");
                }
            } else if (key == "target_fps") {
                try {
                    config.host.targetFps = static_cast<uint32_t>(std::stoul(val));
                } catch (...) {
                    return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Invalid target_fps integer");
                }
            } else if (key == "target_bitrate_kbps") {
                try {
                    config.host.targetBitrateKbps = static_cast<uint32_t>(std::stoul(val));
                } catch (...) {
                    return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Invalid target_bitrate_kbps integer");
                }
            } else if (key == "enable_hw_encode") {
                config.host.enableHardwareEncoding = (val == "true" || val == "1");
            } else if (key == "max_sessions") {
                try {
                    config.host.maxSessions = static_cast<uint32_t>(std::stoul(val));
                } catch (...) {
                    return Error(SubsystemId::HostApp, ErrorCode::InvalidArgument, "Invalid max_sessions integer");
                }
            } else if (key == "allow_remote_input") {
                config.host.allowRemoteInput = (val == "true" || val == "1");
            }
        } else if (currentSection == "client") {
            if (key == "server_address") {
                config.client.serverAddress = val;
            } else if (key == "server_port") {
                try {
                    config.client.serverPort = static_cast<uint16_t>(std::stoul(val));
                } catch (...) {
                    return Error(SubsystemId::ClientApp, ErrorCode::InvalidArgument, "Invalid client server_port integer");
                }
            } else if (key == "low_latency_mode") {
                config.client.lowLatencyMode = (val == "true" || val == "1");
            } else if (key == "fullscreen") {
                config.client.fullscreen = (val == "true" || val == "1");
            } else if (key == "stats_overlay") {
                config.client.statsOverlay = (val == "true" || val == "1");
            }
        }
    }

    auto valRes = config.validate();
    if (!valRes) {
        return valRes.error();
    }

    return config;
}

Result<AppConfig> AppConfig::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Error(SubsystemId::Common, ErrorCode::IoError, "Unable to open configuration file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

Result<void> AppConfig::saveToFile(const std::string& path) const {
    auto valRes = validate();
    if (!valRes) {
        return valRes.error();
    }
    std::ofstream file(path);
    if (!file.is_open()) {
        return Error(SubsystemId::Common, ErrorCode::IoError, "Unable to write configuration file: " + path);
    }
    file << serializeToString();
    return Result<void>::success();
}

} // namespace mist::common
