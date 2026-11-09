#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace mist::ipc {

enum class MessageType : uint16_t {
    Unknown = 0,
    Heartbeat = 1,
    ProcessInitialized = 2,
    SignalingConnected = 3,
    IceNegotiationStarted = 4,
    IceConnected = 5,
    HostApprovalReceived = 6,
    VideoTrackReceived = 7,
    DecoderInitialized = 8,
    FirstFramePresented = 9,
    AudioPlaybackActive = 10,
    TelemetryUpdate = 11,
    UpdateConfig = 12,
    ConfigApplied = 13,
    ConnectionLost = 14,
    StopStreaming = 15,
    StreamingStopped = 16,
    WindowHandle = 17,
    SetViewport = 18,
    StreamingActive = 19
};

#pragma pack(push, 1)
struct IpcHeader {
    uint32_t magic{0x4D495354}; // 'MIST'
    uint16_t version{1};
    uint16_t messageType{0};
    uint32_t payloadSize{0};
    uint64_t timestampNs{0};
};

struct IpcTelemetryPayload {
    double bitrateKbps{0.0};
    double rttMs{0.0};
    double jitterMs{0.0};
    double packetLossPercent{0.0};
    float fps{0.0f};
    uint32_t width{0};
    uint32_t height{0};
    uint64_t framesProcessed{0};
    uint64_t framesDropped{0};
    uint64_t decoderErrors{0};
    uint32_t audioActive{0};
    uint32_t inputActive{0};
};

struct IpcConfigPayload {
    uint32_t bitrateKbps{0};
    uint32_t fps{0};
    uint32_t qualityProfile{0};
    uint32_t success{0};
    char message[128]{0};
};

struct IpcWindowPayload {
    uint64_t hwnd{0};
    uint32_t width{0};
    uint32_t height{0};
};

struct IpcViewportPayload {
    int32_t x{0};
    int32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
};
#pragma pack(pop)

struct IpcMessage {
    MessageType type{MessageType::Heartbeat};
    uint64_t timestampNs{0};
    std::vector<uint8_t> payload;

    template<typename T>
    [[nodiscard]] const T* as() const noexcept {
        if (payload.size() < sizeof(T)) return nullptr;
        return reinterpret_cast<const T*>(payload.data());
    }

    template<typename T>
    [[nodiscard]] const T* getPayload() const noexcept {
        return as<T>();
    }

    template<typename T>
    void setPayload(const T& val) {
        payload.resize(sizeof(T));
        memcpy(payload.data(), &val, sizeof(T));
    }
};

#if defined(_WIN32)

class NamedPipeServer {
public:
    using MessageCallback = std::function<void(const IpcMessage& msg)>;

    NamedPipeServer() noexcept = default;
    ~NamedPipeServer() { shutdown(); }

    bool start(const std::wstring& pipeName, MessageCallback callback) {
        pipeName_ = pipeName;
        callback_ = std::move(callback);
        isRunning_.store(true);

        hPipe_ = CreateNamedPipeW(
            pipeName_.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            65536,
            65536,
            5000,
            nullptr
        );

        if (hPipe_ == INVALID_HANDLE_VALUE) {
            isRunning_.store(false);
            return false;
        }

        ioThread_ = std::thread([this]() { runServerLoop(); });
        return true;
    }

    void shutdown() {
        if (!isRunning_.exchange(false)) return;

        if (hPipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipe_);
            hPipe_ = INVALID_HANDLE_VALUE;
        }

        if (ioThread_.joinable()) {
            ioThread_.join();
        }
    }

    bool sendMessage(const IpcMessage& msg) {
        std::lock_guard<std::mutex> lock(writeMutex_);
        if (hPipe_ == INVALID_HANDLE_VALUE || !isConnected_.load()) return false;

        IpcHeader header{};
        header.messageType = static_cast<uint16_t>(msg.type);
        header.payloadSize = static_cast<uint32_t>(msg.payload.size());
        header.timestampNs = msg.timestampNs > 0 ? msg.timestampNs : getNowNs();

        DWORD written = 0;
        if (!WriteFile(hPipe_, &header, sizeof(header), &written, nullptr) || written != sizeof(header)) {
            return false;
        }

        if (!msg.payload.empty()) {
            if (!WriteFile(hPipe_, msg.payload.data(), static_cast<DWORD>(msg.payload.size()), &written, nullptr) ||
                written != msg.payload.size()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool isConnected() const noexcept { return isConnected_.load(); }

private:
    static uint64_t getNowNs() noexcept {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    void runServerLoop() {
        while (isRunning_.load()) {
            if (hPipe_ == INVALID_HANDLE_VALUE) break;

            BOOL connected = ConnectNamedPipe(hPipe_, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (!connected) {
                if (GetLastError() == ERROR_OPERATION_ABORTED || !isRunning_.load()) break;
                Sleep(50);
                continue;
            }

            isConnected_.store(true);

            while (isRunning_.load()) {
                IpcHeader header{};
                DWORD bytesRead = 0;
                BOOL success = ReadFile(hPipe_, &header, sizeof(header), &bytesRead, nullptr);
                if (!success || bytesRead != sizeof(header)) {
                    break; // Client disconnected
                }

                if (header.magic != 0x4D495354) {
                    break;
                }

                IpcMessage msg{};
                msg.type = static_cast<MessageType>(header.messageType);
                msg.timestampNs = header.timestampNs;

                if (header.payloadSize > 0 && header.payloadSize <= 1048576) { // 1 MB bound
                    msg.payload.resize(header.payloadSize);
                    success = ReadFile(hPipe_, msg.payload.data(), header.payloadSize, &bytesRead, nullptr);
                    if (!success || bytesRead != header.payloadSize) {
                        break;
                    }
                }

                if (callback_) {
                    callback_(msg);
                }
            }

            isConnected_.store(false);
            DisconnectNamedPipe(hPipe_);
        }
    }

    std::wstring pipeName_;
    HANDLE hPipe_{INVALID_HANDLE_VALUE};
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isConnected_{false};
    MessageCallback callback_;
    std::thread ioThread_;
    std::mutex writeMutex_;
};

class NamedPipeClient {
public:
    using MessageCallback = std::function<void(const IpcMessage& msg)>;

    NamedPipeClient() noexcept = default;
    ~NamedPipeClient() { disconnect(); }

    bool connect(const std::wstring& pipeName, MessageCallback callback = nullptr) {
        pipeName_ = pipeName;
        callback_ = std::move(callback);

        for (int attempts = 0; attempts < 10; ++attempts) {
            hPipe_ = CreateFileW(
                pipeName_.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );

            if (hPipe_ != INVALID_HANDLE_VALUE) break;
            Sleep(100);
        }

        if (hPipe_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(hPipe_, &mode, nullptr, nullptr);

        isConnected_.store(true);
        isRunning_.store(true);

        if (callback_) {
            ioThread_ = std::thread([this]() { runReaderLoop(); });
        }
        return true;
    }

    void disconnect() {
        if (!isRunning_.exchange(false)) return;
        isConnected_.store(false);

        if (hPipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipe_);
            hPipe_ = INVALID_HANDLE_VALUE;
        }

        if (ioThread_.joinable()) {
            ioThread_.join();
        }
    }

    bool sendMessage(const IpcMessage& msg) {
        std::lock_guard<std::mutex> lock(writeMutex_);
        if (hPipe_ == INVALID_HANDLE_VALUE || !isConnected_.load()) return false;

        IpcHeader header{};
        header.messageType = static_cast<uint16_t>(msg.type);
        header.payloadSize = static_cast<uint32_t>(msg.payload.size());
        header.timestampNs = msg.timestampNs > 0 ? msg.timestampNs : getNowNs();

        DWORD written = 0;
        if (!WriteFile(hPipe_, &header, sizeof(header), &written, nullptr) || written != sizeof(header)) {
            return false;
        }

        if (!msg.payload.empty()) {
            if (!WriteFile(hPipe_, msg.payload.data(), static_cast<DWORD>(msg.payload.size()), &written, nullptr) ||
                written != msg.payload.size()) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool isConnected() const noexcept { return isConnected_.load(); }

private:
    static uint64_t getNowNs() noexcept {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    void runReaderLoop() {
        while (isRunning_.load()) {
            if (hPipe_ == INVALID_HANDLE_VALUE) break;

            IpcHeader header{};
            DWORD bytesRead = 0;
            BOOL success = ReadFile(hPipe_, &header, sizeof(header), &bytesRead, nullptr);
            if (!success || bytesRead != sizeof(header)) {
                break;
            }

            if (header.magic != 0x4D495354) {
                break;
            }

            IpcMessage msg{};
            msg.type = static_cast<MessageType>(header.messageType);
            msg.timestampNs = header.timestampNs;

            if (header.payloadSize > 0 && header.payloadSize <= 1048576) {
                msg.payload.resize(header.payloadSize);
                success = ReadFile(hPipe_, msg.payload.data(), header.payloadSize, &bytesRead, nullptr);
                if (!success || bytesRead != header.payloadSize) {
                    break;
                }
            }

            if (callback_) {
                callback_(msg);
            }
        }
        isConnected_.store(false);
    }

    std::wstring pipeName_;
    HANDLE hPipe_{INVALID_HANDLE_VALUE};
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isConnected_{false};
    MessageCallback callback_;
    std::thread ioThread_;
    std::mutex writeMutex_;
};

#endif // _WIN32

} // namespace mist::ipc
