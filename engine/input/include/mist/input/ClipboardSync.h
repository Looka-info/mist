#pragma once

#include "mist/common/Result.h"
#include "mist/input/InputTypes.h"
#include "mist/input/InputPermissionManager.h"
#include <string>
#include <string_view>
#include <mutex>
#include <functional>

namespace mist::input {

struct ClipboardSyncConfig {
    bool enabled{true};
    size_t maxTextBytes{MAX_CLIPBOARD_TEXT_BYTES}; // 1 MB limit
    bool simulationOnly{false};                    // Bypasses Win32 clipboard API for tests/headless
};

using OutgoingClipboardCallback = std::function<void(const std::string& text)>;

class ClipboardSyncController {
public:
    explicit ClipboardSyncController(ClipboardSyncConfig config = {}) noexcept;
    ~ClipboardSyncController() = default;

    // Host: process incoming clipboard text from a connected guest
    // Verifies guest clipboard permission, text size and validity, and applies to host clipboard
    [[nodiscard]] common::Result<void> processIncomingGuestClipboard(
        const std::string& guestId,
        const std::string& text,
        InputPermissionManager& permissionManager
    );

    // Host or Client: prepare an outgoing clipboard sync message from local clipboard
    // Performs echo suppression: returns false if text is identical to last received remote content
    [[nodiscard]] common::Result<std::string> prepareOutgoingClipboard(const std::string& localText);

    // Simulation/Test accessor: get currently stored clipboard text
    [[nodiscard]] std::string getSimulatedClipboardText() const;
    void setSimulatedClipboardText(std::string text);

    // Hashes & echo suppression inspection
    [[nodiscard]] uint64_t getLastSentHash() const noexcept;
    [[nodiscard]] uint64_t getLastReceivedHash() const noexcept;
    void resetEchoFilters() noexcept;

    // Configuration
    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    void setOutgoingCallback(OutgoingClipboardCallback cb);

    // Helper: validate UTF-8 text string
    [[nodiscard]] static bool isValidUtf8(std::string_view sv) noexcept;

    // Helper: compute 64-bit FNV-1a hash
    [[nodiscard]] static uint64_t computeHash(std::string_view sv) noexcept;

private:
    [[nodiscard]] common::Result<std::string> readSystemClipboard();
    [[nodiscard]] common::Result<void> writeSystemClipboard(const std::string& text);

    mutable std::mutex mutex_;
    ClipboardSyncConfig config_;
    std::string simulatedBuffer_;
    uint64_t lastSentHash_{0};
    uint64_t lastReceivedHash_{0};
    OutgoingClipboardCallback outgoingCallback_;
};

} // namespace mist::input
