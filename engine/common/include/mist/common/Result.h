#pragma once

#include "Error.h"
#include <variant>
#include <optional>
#include <stdexcept>
#include <utility>

namespace mist::common {

template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : storage_(std::move(value)) {}

    Result(Error error) noexcept
        : storage_(std::move(error)) {}

    [[nodiscard]] bool hasValue() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] bool hasError() const noexcept {
        return std::holds_alternative<Error>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] T& value() & {
        if (hasError()) {
            throw std::runtime_error("Attempted to access value of an Error Result: " + error().format());
        }
        return std::get<T>(storage_);
    }

    [[nodiscard]] const T& value() const & {
        if (hasError()) {
            throw std::runtime_error("Attempted to access value of an Error Result: " + error().format());
        }
        return std::get<T>(storage_);
    }

    [[nodiscard]] T&& value() && {
        if (hasError()) {
            throw std::runtime_error("Attempted to access value of an Error Result: " + error().format());
        }
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] const Error& error() const noexcept {
        return std::get<Error>(storage_);
    }

    template <typename U>
    [[nodiscard]] T valueOr(U&& defaultValue) const& {
        return hasValue() ? std::get<T>(storage_) : static_cast<T>(std::forward<U>(defaultValue));
    }

    template <typename U>
    [[nodiscard]] T valueOr(U&& defaultValue) && {
        return hasValue() ? std::get<T>(std::move(storage_)) : static_cast<T>(std::forward<U>(defaultValue));
    }

private:
    std::variant<T, Error> storage_;
};

// Specialization for void
template <>
class [[nodiscard]] Result<void> {
public:
    Result() noexcept : error_(std::nullopt) {}
    Result(Error error) noexcept : error_(std::move(error)) {}

    [[nodiscard]] bool hasValue() const noexcept {
        return !error_.has_value();
    }

    [[nodiscard]] bool hasError() const noexcept {
        return error_.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return hasValue();
    }

    [[nodiscard]] const Error& error() const noexcept {
        return *error_;
    }

    static Result<void> success() noexcept {
        return Result<void>();
    }

private:
    std::optional<Error> error_;
};

} // namespace mist::common
