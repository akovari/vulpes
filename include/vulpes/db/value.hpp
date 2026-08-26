#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace vulpes::db {

using Blob = std::vector<std::byte>;

class Value {
public:
    using Storage = std::variant<std::monostate, std::int64_t, double, std::string, Blob>;

    Value() = default;
    Value(std::nullptr_t) noexcept {}
    Value(bool value) noexcept;
    Value(std::int64_t value) noexcept;
    Value(int value) noexcept;
    Value(double value) noexcept;
    Value(std::string value);
    Value(const char* value);
    Value(Blob value);

    [[nodiscard]] auto is_null() const noexcept -> bool;
    [[nodiscard]] auto as_int() const -> std::int64_t;
    [[nodiscard]] auto as_double() const -> double;
    [[nodiscard]] auto as_string() const -> const std::string&;
    [[nodiscard]] auto as_blob() const -> std::span<const std::byte>;
    [[nodiscard]] auto storage() const noexcept -> const Storage& { return storage_; }
    auto operator==(const Value&) const -> bool = default;

private:
    Storage storage_{};
};

} // namespace vulpes::db
