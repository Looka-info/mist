#include "mist/input/ClipboardSync.h"
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mist::input {

namespace {

constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME  = 1099511628211ULL;

} // namespace

ClipboardSyncController::ClipboardSyncController(ClipboardSyncConfig config) noexcept
    : config_(config) {}

bool ClipboardSyncController::isValidUtf8(std::string_view sv) noexcept {
    const auto* bytes = reinterpret_cast<const uint8_t*>(sv.data());
    size_t len = sv.size();
    size_t i = 0;

    while (i < len) {
        if (bytes[i] <= 0x7F) {
            // Null characters are rejected in text clipboard
            if (bytes[i] == '\0') {
                return false;
            }
            i += 1;
        } else if ((bytes[i] & 0xE0) == 0xC0) {
            if (i + 1 >= len || (bytes[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((bytes[i] & 0xF0) == 0xE0) {
            if (i + 2 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((bytes[i] & 0xF8) == 0xF0) {
            if (i + 3 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

uint64_t ClipboardSyncController::computeHash(std::string_view sv) noexcept {
    uint64_t hash = FNV_OFFSET;
    for (char c : sv) {
        hash ^= static_cast<uint8_t>(c);
        hash *= FNV_PRIME;
    }
    return hash;
}

common::Result<void> ClipboardSyncController::processIncomingGuestClipboard(
    const std::string& guestId,
    const std::string& text,
    InputPermissionManager& permissionManager
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.enabled) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidState,
                             "Clipboard synchronization is disabled");
    }

    if (!permissionManager.isGuestInputAllowed(guestId, InputType::ClipboardEvent)) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::PermissionDenied,
                             "Guest is not granted clipboard permission");
    }

    if (text.size() > config_.maxTextBytes) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Clipboard text exceeds maximum size limit");
    }

    if (!isValidUtf8(text)) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Clipboard text is not valid UTF-8");
    }

    uint64_t hash = computeHash(text);
    if (hash == lastSentHash_) {
        // Echo suppression: local machine sent this recently, ignore remote bounce
        return common::Result<void>::success();
    }

    lastReceivedHash_ = hash;
    return writeSystemClipboard(text);
}

common::Result<std::string> ClipboardSyncController::prepareOutgoingClipboard(const std::string& localText) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.enabled) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidState,
                             "Clipboard synchronization is disabled");
    }

    if (localText.empty()) {
        return std::string("");
    }

    if (localText.size() > config_.maxTextBytes) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Outgoing clipboard text exceeds maximum size limit");
    }

    if (!isValidUtf8(localText)) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InvalidArgument,
                             "Outgoing clipboard text is not valid UTF-8");
    }

    uint64_t hash = computeHash(localText);
    if (hash == lastReceivedHash_) {
        // Echo suppression: text originated from remote sync, do not re-send
        return common::Error(common::SubsystemId::Input, common::ErrorCode::AlreadyExists,
                             "Clipboard text matches last received remote content (echo suppressed)");
    }

    lastSentHash_ = hash;

    if (outgoingCallback_) {
        outgoingCallback_(localText);
    }

    return localText;
}

std::string ClipboardSyncController::getSimulatedClipboardText() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return simulatedBuffer_;
}

void ClipboardSyncController::setSimulatedClipboardText(std::string text) {
    std::lock_guard<std::mutex> lock(mutex_);
    simulatedBuffer_ = std::move(text);
}

uint64_t ClipboardSyncController::getLastSentHash() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastSentHash_;
}

uint64_t ClipboardSyncController::getLastReceivedHash() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastReceivedHash_;
}

void ClipboardSyncController::resetEchoFilters() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    lastSentHash_ = 0;
    lastReceivedHash_ = 0;
}

void ClipboardSyncController::setEnabled(bool enabled) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.enabled = enabled;
}

bool ClipboardSyncController::isEnabled() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.enabled;
}

void ClipboardSyncController::setOutgoingCallback(OutgoingClipboardCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    outgoingCallback_ = std::move(cb);
}

common::Result<std::string> ClipboardSyncController::readSystemClipboard() {
    if (config_.simulationOnly) {
        return simulatedBuffer_;
    }

#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InternalError, "Failed to open Win32 clipboard");
    }

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return std::string("");
    }

    const auto* wstr = static_cast<const wchar_t*>(GlobalLock(hData));
    if (!wstr) {
        CloseClipboard();
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InternalError, "Failed to lock clipboard data");
    }

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string result;
    if (utf8Len > 1) {
        result.resize(static_cast<size_t>(utf8Len - 1));
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), utf8Len, nullptr, nullptr);
    }

    GlobalUnlock(hData);
    CloseClipboard();
    return result;
#else
    return simulatedBuffer_;
#endif
}

common::Result<void> ClipboardSyncController::writeSystemClipboard(const std::string& text) {
    if (config_.simulationOnly) {
        simulatedBuffer_ = text;
        return common::Result<void>::success();
    }

#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        return common::Error(common::SubsystemId::Input, common::ErrorCode::InternalError, "Failed to open Win32 clipboard");
    }

    EmptyClipboard();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wlen > 0) {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, static_cast<size_t>(wlen) * sizeof(wchar_t));
        if (hMem) {
            auto* pDest = static_cast<wchar_t*>(GlobalLock(hMem));
            if (pDest) {
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pDest, wlen);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            } else {
                GlobalFree(hMem);
            }
        }
    }

    CloseClipboard();
    return common::Result<void>::success();
#else
    simulatedBuffer_ = text;
    return common::Result<void>::success();
#endif
}

} // namespace mist::input
