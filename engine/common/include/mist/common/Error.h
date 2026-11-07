#pragma once

#include "Subsystem.h"
#include <string>
#include <string_view>
#include <cstdint>

namespace mist::common {

enum class ErrorCode : uint32_t {
    Success = 0,
    InvalidArgument,
    InvalidState,
    NotInitialized,
    AlreadyInitialized,
    Timeout,
    NotFound,
    AlreadyExists,
    PermissionDenied,
    ResourceExhausted,
    IoError,
    NetworkError,
    HardwareUnavailable,
    FormatNotSupported,
    AuthenticationFailed,
    NotImplemented,
    InternalError
};

[[nodiscard]] constexpr std::string_view errorCodeToString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success:              return "Success";
        case ErrorCode::InvalidArgument:      return "InvalidArgument";
        case ErrorCode::InvalidState:        return "InvalidState";
        case ErrorCode::NotInitialized:       return "NotInitialized";
        case ErrorCode::AlreadyInitialized:   return "AlreadyInitialized";
        case ErrorCode::Timeout:              return "Timeout";
        case ErrorCode::NotFound:             return "NotFound";
        case ErrorCode::AlreadyExists:        return "AlreadyExists";
        case ErrorCode::PermissionDenied:     return "PermissionDenied";
        case ErrorCode::ResourceExhausted:    return "ResourceExhausted";
        case ErrorCode::IoError:              return "IoError";
        case ErrorCode::NetworkError:         return "NetworkError";
        case ErrorCode::HardwareUnavailable:  return "HardwareUnavailable";
        case ErrorCode::FormatNotSupported:   return "FormatNotSupported";
        case ErrorCode::AuthenticationFailed: return "AuthenticationFailed";
        case ErrorCode::NotImplemented:       return "NotImplemented";
        case ErrorCode::InternalError:        return "InternalError";
    }
    return "UnknownError";
}

class Error {
public:
    Error() noexcept = default;

    Error(SubsystemId subsystem, ErrorCode code, std::string message, int32_t platformCode = 0) noexcept
        : subsystem_(subsystem), code_(code), message_(std::move(message)), platformCode_(platformCode) {}

    [[nodiscard]] SubsystemId subsystem() const noexcept { return subsystem_; }
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] int32_t platformCode() const noexcept { return platformCode_; }

    [[nodiscard]] bool isSuccess() const noexcept { return code_ == ErrorCode::Success; }
    [[nodiscard]] explicit operator bool() const noexcept { return !isSuccess(); }

    [[nodiscard]] std::string format() const;

private:
    SubsystemId subsystem_{SubsystemId::Common};
    ErrorCode code_{ErrorCode::Success};
    std::string message_;
    int32_t platformCode_{0};
};

} // namespace mist::common
