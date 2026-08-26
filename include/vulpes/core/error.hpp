#pragma once

#include <stdexcept>
#include <string>

namespace vulpes {

enum class ErrorCategory { database, constraint, validation, io, metadata, script, terminal };

class Error : public std::runtime_error {
public:
    Error(ErrorCategory category, std::string message, int native_code = 0)
        : std::runtime_error{std::move(message)}, category_{category}, native_code_{native_code} {}

    [[nodiscard]] auto category() const noexcept -> ErrorCategory { return category_; }
    [[nodiscard]] auto native_code() const noexcept -> int { return native_code_; }

private:
    ErrorCategory category_;
    int native_code_;
};

} // namespace vulpes

