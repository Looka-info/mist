#include "mist/security/PairingManager.h"
#include "mist/security/Security.h"
#include "mist/common/Logger.h"
#include <random>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace mist::security {

PairingManager::PairingManager() noexcept = default;

uint64_t PairingManager::getNowEpochSec() const noexcept {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

common::Result<std::string> PairingManager::generatePairingCode(uint32_t lifetimeSec, uint64_t nowEpochSec) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nowEpochSec == 0) nowEpochSec = getNowEpochSec();

    // 32-symbol unambiguous charset (excludes 0, O, 1, I) providing 30 bits of entropy
    static constexpr char charset[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string code;
    code.reserve(8);
    for (int i = 0; i < 3; ++i) code += charset[dist(gen)];
    code += '-';
    for (int i = 0; i < 3; ++i) code += charset[dist(gen)];

    activeCode_ = code;
    codeExpiresAt_ = nowEpochSec + lifetimeSec;
    failedAttempts_ = 0;
    pendingRequests_.clear();

    MIST_LOG_INFO(common::SubsystemId::Security, "Generated new pairing code: " + activeCode_ +
                  " (expires in " + std::to_string(lifetimeSec) + "s)");

    return activeCode_;
}

common::Result<void> PairingManager::submitPairingRequest(const PairingRequest& request, uint64_t nowEpochSec) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nowEpochSec == 0) nowEpochSec = getNowEpochSec();

    if (nowEpochSec < lockoutUntilEpochSec_) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::PermissionDenied,
                             "Pairing locked due to excessive failed attempts. Please retry later.");
    }

    if (activeCode_.empty() || nowEpochSec > codeExpiresAt_) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::ResourceExhausted,
                             "Pairing code has expired or is not active.");
    }

    if (!constantTimeEquals(request.pairingCode, activeCode_)) {
        failedAttempts_++;
        MIST_LOG_WARN(common::SubsystemId::Security,
                      "Invalid pairing code attempt (" + std::to_string(failedAttempts_) + "/" +
                      std::to_string(kMaxFailedAttempts) + ")");
        if (failedAttempts_ >= kMaxFailedAttempts) {
            lockoutUntilEpochSec_ = nowEpochSec + kLockoutDurationSec;
            MIST_LOG_ERROR(common::SubsystemId::Security, "Pairing rate limit triggered; lockout active.");
        }
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InvalidArgument,
                             "Invalid pairing code.");
    }

    if (request.deviceId.empty()) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InvalidArgument,
                             "Client device ID cannot be empty.");
    }

    PairingRequest req = request;
    req.requestEpochSec = nowEpochSec;
    pendingRequests_[request.deviceId] = req;

    MIST_LOG_INFO(common::SubsystemId::Security, "Pairing request queued from device: " + request.deviceName +
                  " (" + request.deviceId + ")");

    return common::Result<void>::success();
}

common::Result<void> PairingManager::approvePairingRequest(
    const std::string& deviceId,
    const DevicePermissions& permissions,
    uint64_t nowEpochSec
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (nowEpochSec == 0) nowEpochSec = getNowEpochSec();

    auto it = pendingRequests_.find(deviceId);
    if (it == pendingRequests_.end()) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::NotFound,
                             "No pending pairing request found for device: " + deviceId);
    }

    TrustedDevice trusted{};
    trusted.deviceId = it->second.deviceId;
    trusted.deviceName = it->second.deviceName;
    trusted.publicKeyFingerprint = it->second.publicKeyFingerprint;
    trusted.pairedAtEpochSec = nowEpochSec;
    trusted.lastConnectedEpochSec = nowEpochSec;
    trusted.permissions = permissions;

    trustedDevices_[deviceId] = trusted;
    pendingRequests_.erase(it);

    // Invalidate single-use code upon successful approval
    activeCode_.clear();
    codeExpiresAt_ = 0;

    MIST_LOG_INFO(common::SubsystemId::Security, "Pairing approved for device: " + trusted.deviceName +
                  " (" + deviceId + ")");

    return common::Result<void>::success();
}

common::Result<void> PairingManager::rejectPairingRequest(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pendingRequests_.find(deviceId);
    if (it != pendingRequests_.end()) {
        pendingRequests_.erase(it);
        MIST_LOG_INFO(common::SubsystemId::Security, "Pairing request rejected for device: " + deviceId);
        return common::Result<void>::success();
    }
    return common::Error(common::SubsystemId::Security, common::ErrorCode::NotFound, "Request not found");
}

common::Result<void> PairingManager::revokeDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trustedDevices_.find(deviceId);
    if (it != trustedDevices_.end()) {
        trustedDevices_.erase(it);
        MIST_LOG_INFO(common::SubsystemId::Security, "Revoked trust for device: " + deviceId);
        return common::Result<void>::success();
    }
    return common::Error(common::SubsystemId::Security, common::ErrorCode::NotFound, "Device not found in trusted store");
}

bool PairingManager::isDeviceTrusted(const std::string& deviceId, const std::string& fingerprint) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trustedDevices_.find(deviceId);
    if (it == trustedDevices_.end()) return false;
    if (!fingerprint.empty()) {
        return constantTimeEquals(it->second.publicKeyFingerprint, fingerprint);
    }
    return true;
}

common::Result<DevicePermissions> PairingManager::getDevicePermissions(const std::string& deviceId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trustedDevices_.find(deviceId);
    if (it != trustedDevices_.end()) {
        return it->second.permissions;
    }
    return common::Error(common::SubsystemId::Security, common::ErrorCode::NotFound, "Device not found");
}

common::Result<void> PairingManager::updateDevicePermissions(const std::string& deviceId, const DevicePermissions& perms) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trustedDevices_.find(deviceId);
    if (it != trustedDevices_.end()) {
        it->second.permissions = perms;
        return common::Result<void>::success();
    }
    return common::Error(common::SubsystemId::Security, common::ErrorCode::NotFound, "Device not found");
}

std::vector<TrustedDevice> PairingManager::getTrustedDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TrustedDevice> list;
    list.reserve(trustedDevices_.size());
    for (const auto& [_, dev] : trustedDevices_) {
        list.push_back(dev);
    }
    return list;
}

std::vector<PairingRequest> PairingManager::getPendingRequests() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PairingRequest> list;
    list.reserve(pendingRequests_.size());
    for (const auto& [_, req] : pendingRequests_) {
        list.push_back(req);
    }
    return list;
}

std::string PairingManager::getActivePairingCode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeCode_;
}

uint64_t PairingManager::getCodeExpirationEpochSec() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return codeExpiresAt_;
}

void PairingManager::reset() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    activeCode_.clear();
    codeExpiresAt_ = 0;
    failedAttempts_ = 0;
    lockoutUntilEpochSec_ = 0;
    pendingRequests_.clear();
    trustedDevices_.clear();
}

common::Result<void> PairingManager::saveTrustedDevices(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string tmpPath = filePath + ".tmp";
    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out.is_open()) {
        return common::Error(common::SubsystemId::Security, common::ErrorCode::InternalError,
                             "Failed to open file for writing: " + tmpPath);
    }

    out << "[\n";
    bool first = true;
    for (const auto& [_, dev] : trustedDevices_) {
        if (!first) out << ",\n";
        first = false;
        out << "  {\n"
            << "    \"device_id\": \"" << dev.deviceId << "\",\n"
            << "    \"device_name\": \"" << dev.deviceName << "\",\n"
            << "    \"fingerprint\": \"" << dev.publicKeyFingerprint << "\",\n"
            << "    \"paired_at\": " << dev.pairedAtEpochSec << ",\n"
            << "    \"last_connected\": " << dev.lastConnectedEpochSec << ",\n"
            << "    \"allow_video\": " << (dev.permissions.allowVideo ? "true" : "false") << ",\n"
            << "    \"allow_audio\": " << (dev.permissions.allowAudio ? "true" : "false") << ",\n"
            << "    \"allow_keyboard\": " << (dev.permissions.allowKeyboard ? "true" : "false") << ",\n"
            << "    \"allow_mouse\": " << (dev.permissions.allowMouse ? "true" : "false") << ",\n"
            << "    \"allow_gamepad\": " << (dev.permissions.allowGamepad ? "true" : "false") << "\n"
            << "  }";
    }
    out << "\n]\n";
    out.close();

    // Atomic replacement to prevent corruption on sudden termination
#if defined(_WIN32)
    if (!MoveFileExA(tmpPath.c_str(), filePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        // Fallback to std::filesystem
        std::error_code ec;
        std::filesystem::rename(tmpPath, filePath, ec);
    }
#else
    std::error_code ec;
    std::filesystem::rename(tmpPath, filePath, ec);
#endif

    return common::Result<void>::success();
}

common::Result<void> PairingManager::loadTrustedDevices(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream in(filePath);
    if (!in.is_open()) {
        return common::Result<void>::success(); // Clean start if file does not yet exist
    }

    std::string line;
    TrustedDevice cur{};
    bool insideObject = false;

    auto extractStr = [](const std::string& l, const std::string& key) -> std::string {
        auto pos = l.find("\"" + key + "\":");
        if (pos == std::string::npos) return "";
        auto start = l.find('"', pos + key.length() + 3);
        if (start == std::string::npos) return "";
        auto end = l.find('"', start + 1);
        if (end == std::string::npos) return "";
        return l.substr(start + 1, end - start - 1);
    };

    auto extractBool = [](const std::string& l, const std::string& key) -> bool {
        auto pos = l.find("\"" + key + "\":");
        if (pos == std::string::npos) return false;
        return l.find("true", pos) != std::string::npos;
    };

    while (std::getline(in, line)) {
        if (line.find('{') != std::string::npos) {
            insideObject = true;
            cur = TrustedDevice{};
        } else if (line.find('}') != std::string::npos && insideObject) {
            if (!cur.deviceId.empty()) {
                trustedDevices_[cur.deviceId] = cur;
            }
            insideObject = false;
        } else if (insideObject) {
            if (line.find("\"device_id\":") != std::string::npos) cur.deviceId = extractStr(line, "device_id");
            if (line.find("\"device_name\":") != std::string::npos) cur.deviceName = extractStr(line, "device_name");
            if (line.find("\"fingerprint\":") != std::string::npos) cur.publicKeyFingerprint = extractStr(line, "fingerprint");
            if (line.find("\"allow_video\":") != std::string::npos) cur.permissions.allowVideo = extractBool(line, "allow_video");
            if (line.find("\"allow_audio\":") != std::string::npos) cur.permissions.allowAudio = extractBool(line, "allow_audio");
            if (line.find("\"allow_keyboard\":") != std::string::npos) cur.permissions.allowKeyboard = extractBool(line, "allow_keyboard");
            if (line.find("\"allow_mouse\":") != std::string::npos) cur.permissions.allowMouse = extractBool(line, "allow_mouse");
            if (line.find("\"allow_gamepad\":") != std::string::npos) cur.permissions.allowGamepad = extractBool(line, "allow_gamepad");
        }
    }

    MIST_LOG_INFO(common::SubsystemId::Security, "Loaded " + std::to_string(trustedDevices_.size()) + " trusted devices.");
    return common::Result<void>::success();
}

} // namespace mist::security
