#pragma once

#include "mist/common/Result.h"
#include "mist/security/PairingManager.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace mist::security {

struct RemotePairingRequestMessage {
    std::string deviceId;
    std::string deviceName;
    std::string pairingCode;
    std::string publicKeyFingerprint;
    uint64_t requestEpochSec{0};
};

struct RemotePairingResponseMessage {
    bool approved{false};
    std::string authToken;
    std::string reason;
    DevicePermissions permissions{};
    std::string hostName;
};

// Host-side pairing server that receives remote client pairing requests,
// validates them against the authoritative Host PairingManager, and returns approvals.
class RemotePairingServer {
public:
    using RequestReceivedCallback = std::function<void(const RemotePairingRequestMessage& req)>;

    RemotePairingServer(PairingManager& manager) noexcept
        : pairingManager_(manager) {}

    ~RemotePairingServer() {
        stop();
    }

    [[nodiscard]] common::Result<void> start(uint16_t port = 8085) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isRunning_.load()) return common::Result<void>::success();

#if defined(_WIN32)
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "WSAStartup failed");
        }

        listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket_ == INVALID_SOCKET) {
            WSACleanup();
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "Failed to create pairing server socket");
        }

        BOOL reuse = TRUE;
        setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
            WSACleanup();
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "Failed to bind pairing server socket to port " + std::to_string(port));
        }

        if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
            WSACleanup();
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "Failed to listen on pairing server socket");
        }

        port_ = port;
        isRunning_.store(true);
        serverThread_ = std::thread([this]() { runServerLoop(); });
        return common::Result<void>::success();
#else
        return common::Error(common::SubsystemId::Security, common::ErrorCode::NotImplemented,
                             "Platform not supported");
#endif
    }

    void stop() {
        if (!isRunning_.exchange(false)) return;

#if defined(_WIN32)
        if (listenSocket_ != INVALID_SOCKET) {
            closesocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
        }
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        WSACleanup();
#endif
    }

    void setRequestCallback(RequestReceivedCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        requestCallback_ = std::move(cb);
    }

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(); }
    [[nodiscard]] uint16_t getPort() const noexcept { return port_; }

private:
#if defined(_WIN32)
    void runServerLoop() {
        while (isRunning_.load()) {
            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            SOCKET clientSock = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
            if (clientSock == INVALID_SOCKET) {
                if (!isRunning_.load()) break;
                Sleep(50);
                continue;
            }

            handleClientConnection(clientSock);
        }
    }

    void handleClientConnection(SOCKET clientSock) {
        char buffer[4096]{0};
        int received = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            closesocket(clientSock);
            return;
        }

        std::string reqStr(buffer, static_cast<size_t>(received));
        RemotePairingRequestMessage req = parseRequest(reqStr);

        RemotePairingResponseMessage resp{};

        // Validate via Host PairingManager
        PairingRequest pr{};
        pr.deviceId = req.deviceId;
        pr.deviceName = req.deviceName;
        pr.pairingCode = req.pairingCode;
        pr.publicKeyFingerprint = req.publicKeyFingerprint;

        auto subRes = pairingManager_.submitPairingRequest(pr);
        if (!subRes.hasValue()) {
            resp.approved = false;
            resp.reason = subRes.error().message();
        } else {
            // Request successfully queued in host's pending requests.
            // Notify UI if callback set
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (requestCallback_) {
                    requestCallback_(req);
                }
            }

            // Await approval or rejection with timeout (up to 45 seconds)
            auto startWait = std::chrono::steady_clock::now();
            bool decided = false;

            while (isRunning_.load()) {
                if (pairingManager_.isDeviceTrusted(req.deviceId)) {
                    resp.approved = true;
                    auto permsRes = pairingManager_.getDevicePermissions(req.deviceId);
                    if (permsRes.hasValue()) {
                        resp.permissions = permsRes.value();
                    }
                    resp.authToken = "mist-auth-" + req.deviceId;
                    resp.hostName = "Mist Host";
                    decided = true;
                    break;
                }

                // Check if still in pending
                bool stillPending = false;
                for (const auto& p : pairingManager_.getPendingRequests()) {
                    if (p.deviceId == req.deviceId) {
                        stillPending = true;
                        break;
                    }
                }

                if (!stillPending) {
                    // Was rejected by host user
                    resp.approved = false;
                    resp.reason = "Pairing request was denied by the host.";
                    decided = true;
                    break;
                }

                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startWait).count();
                if (elapsed > 45) {
                    (void)pairingManager_.rejectPairingRequest(req.deviceId);
                    resp.approved = false;
                    resp.reason = "Pairing request timed out awaiting host approval.";
                    decided = true;
                    break;
                }

                Sleep(100);
            }

            if (!decided) {
                resp.approved = false;
                resp.reason = "Pairing aborted.";
            }
        }

        std::string respStr = serializeResponse(resp);
        send(clientSock, respStr.c_str(), static_cast<int>(respStr.size()), 0);
        closesocket(clientSock);
    }
#endif

    static RemotePairingRequestMessage parseRequest(const std::string& str) {
        RemotePairingRequestMessage req{};
        auto getField = [&str](const std::string& key) -> std::string {
            std::string needle = key + "=";
            auto pos = str.find(needle);
            if (pos == std::string::npos) return "";
            pos += needle.size();
            auto endPos = str.find('\n', pos);
            if (endPos == std::string::npos) endPos = str.size();
            return str.substr(pos, endPos - pos);
        };

        req.deviceId = getField("device_id");
        req.deviceName = getField("device_name");
        req.pairingCode = getField("pairing_code");
        req.publicKeyFingerprint = getField("public_key");
        return req;
    }

    static std::string serializeResponse(const RemotePairingResponseMessage& resp) {
        std::string s;
        s += "approved=" + std::string(resp.approved ? "true" : "false") + "\n";
        s += "auth_token=" + resp.authToken + "\n";
        s += "reason=" + resp.reason + "\n";
        s += "host_name=" + resp.hostName + "\n";
        s += "allow_audio=" + std::to_string(resp.permissions.allowAudio ? 1 : 0) + "\n";
        s += "allow_keyboard=" + std::to_string(resp.permissions.allowKeyboard ? 1 : 0) + "\n";
        s += "allow_mouse=" + std::to_string(resp.permissions.allowMouse ? 1 : 0) + "\n";
        s += "allow_gamepad=" + std::to_string(resp.permissions.allowGamepad ? 1 : 0) + "\n";
        return s;
    }

    PairingManager& pairingManager_;
    std::atomic<bool> isRunning_{false};
    uint16_t port_{8085};
    std::thread serverThread_;
    std::mutex mutex_;
    RequestReceivedCallback requestCallback_;

#if defined(_WIN32)
    SOCKET listenSocket_{INVALID_SOCKET};
#endif
};

// Client-side pairing client that submits a pairing request to the remote host
// and awaits explicit host approval before allowing the stream session to proceed.
class RemotePairingClient {
public:
    RemotePairingClient() noexcept = default;
    ~RemotePairingClient() = default;

    [[nodiscard]] common::Result<RemotePairingResponseMessage> submitPairingRequest(
        const std::string& hostAddress,
        uint16_t hostPort,
        const RemotePairingRequestMessage& request
    ) {
#if defined(_WIN32)
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "WSAStartup failed");
        }

        SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSock == INVALID_SOCKET) {
            WSACleanup();
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "Failed to create client socket");
        }

        DWORD timeout = 50000; // 50s timeout
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(hostPort);
        inet_pton(AF_INET, hostAddress.c_str(), &serverAddr.sin_addr);

        if (connect(clientSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(clientSock);
            WSACleanup();
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "Could not connect to host at " + hostAddress + ":" + std::to_string(hostPort) +
                                 ". Ensure the host is running Wi-Fi sharing.");
        }

        std::string reqPayload;
        reqPayload += "device_id=" + request.deviceId + "\n";
        reqPayload += "device_name=" + request.deviceName + "\n";
        reqPayload += "pairing_code=" + request.pairingCode + "\n";
        reqPayload += "public_key=" + request.publicKeyFingerprint + "\n";

        send(clientSock, reqPayload.c_str(), static_cast<int>(reqPayload.size()), 0);

        char buffer[4096]{0};
        int received = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        closesocket(clientSock);
        WSACleanup();

        if (received <= 0) {
            return common::Error(common::SubsystemId::Security, common::ErrorCode::NetworkError,
                                 "No response from host during pairing request.");
        }

        std::string respStr(buffer, static_cast<size_t>(received));
        RemotePairingResponseMessage resp = parseResponse(respStr);
        return resp;
#else
        return common::Error(common::SubsystemId::Security, common::ErrorCode::NotImplemented,
                             "Platform not supported");
#endif
    }

private:
    static RemotePairingResponseMessage parseResponse(const std::string& str) {
        RemotePairingResponseMessage resp{};
        auto getField = [&str](const std::string& key) -> std::string {
            std::string needle = key + "=";
            auto pos = str.find(needle);
            if (pos == std::string::npos) return "";
            pos += needle.size();
            auto endPos = str.find('\n', pos);
            if (endPos == std::string::npos) endPos = str.size();
            return str.substr(pos, endPos - pos);
        };

        resp.approved = (getField("approved") == "true");
        resp.authToken = getField("auth_token");
        resp.reason = getField("reason");
        resp.hostName = getField("host_name");
        resp.permissions.allowAudio = (getField("allow_audio") == "1");
        resp.permissions.allowKeyboard = (getField("allow_keyboard") == "1");
        resp.permissions.allowMouse = (getField("allow_mouse") == "1");
        resp.permissions.allowGamepad = (getField("allow_gamepad") == "1");
        return resp;
    }
};

} // namespace mist::security
