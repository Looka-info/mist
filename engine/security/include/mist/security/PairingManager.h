#pragma once

#include "mist/common/Result.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace mist::security {

enum class PairingState : uint8_t {
    Idle = 0,
    CodeGenerated,
    RequestPending,
    Paired,
    Revoked,
    Expired
};

struct DevicePermissions {
    bool allowVideo{true};
    bool allowAudio{true};
    bool allowKeyboard{false};
    bool allowMouse{false};
    bool allowGamepad{false};
};

struct TrustedDevice {
    std::string deviceId;
    std::string deviceName;
    std::string publicKeyFingerprint;
    uint64_t pairedAtEpochSec{0};
    uint64_t lastConnectedEpochSec{0};
    DevicePermissions permissions{};
};

struct PairingRequest {
    std::string deviceId;
    std::string deviceName;
    std::string publicKeyFingerprint;
    std::string pairingCode;
    uint64_t requestEpochSec{0};
};

class PairingManager {
public:
    PairingManager() noexcept;
    ~PairingManager() = default;

    // Generates a 6-digit alphanumeric pairing code (e.g. "MIST-829")
    [[nodiscard]] common::Result<std::string> generatePairingCode(uint32_t lifetimeSec = 300, uint64_t nowEpochSec = 0);

    // Remote client submits pairing request with code and identity
    [[nodiscard]] common::Result<void> submitPairingRequest(const PairingRequest& request, uint64_t nowEpochSec = 0);

    // Host user explicitly approves pending request
    [[nodiscard]] common::Result<void> approvePairingRequest(
        const std::string& deviceId,
        const DevicePermissions& permissions = {},
        uint64_t nowEpochSec = 0
    );

    // Host user rejects pending request
    [[nodiscard]] common::Result<void> rejectPairingRequest(const std::string& deviceId);

    // Revokes an existing trusted device
    [[nodiscard]] common::Result<void> revokeDevice(const std::string& deviceId);

    [[nodiscard]] bool isDeviceTrusted(const std::string& deviceId, const std::string& fingerprint = "") const noexcept;
    [[nodiscard]] common::Result<DevicePermissions> getDevicePermissions(const std::string& deviceId) const noexcept;
    [[nodiscard]] common::Result<void> updateDevicePermissions(const std::string& deviceId, const DevicePermissions& perms);

    [[nodiscard]] std::vector<TrustedDevice> getTrustedDevices() const;
    [[nodiscard]] std::vector<PairingRequest> getPendingRequests() const;
    [[nodiscard]] std::string getActivePairingCode() const;
    [[nodiscard]] uint64_t getCodeExpirationEpochSec() const noexcept;

    // Persistence to JSON file
    [[nodiscard]] common::Result<void> loadTrustedDevices(const std::string& filePath);
    [[nodiscard]] common::Result<void> saveTrustedDevices(const std::string& filePath) const;

    void reset() noexcept;

private:
    [[nodiscard]] uint64_t getNowEpochSec() const noexcept;

    mutable std::mutex mutex_;
    std::string activeCode_;
    uint64_t codeExpiresAt_{0};
    uint32_t failedAttempts_{0};
    uint64_t lockoutUntilEpochSec_{0};

    std::unordered_map<std::string, PairingRequest> pendingRequests_;
    std::unordered_map<std::string, TrustedDevice> trustedDevices_;

    static constexpr uint32_t kMaxFailedAttempts = 5;
    static constexpr uint64_t kLockoutDurationSec = 60;
};

} // namespace mist::security
