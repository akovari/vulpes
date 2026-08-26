#include "vulpes/db/value.hpp"

#include <stdexcept>

namespace vulpes::db {

Value::Value(bool value) noexcept : storage_{std::int64_t{value ? 1 : 0}} {}
Value::Value(std::int64_t value) noexcept : storage_{value} {}
Value::Value(int value) noexcept : storage_{static_cast<std::int64_t>(value)} {}
Value::Value(double value) noexcept : storage_{value} {}
Value::Value(std::string value) : storage_{std::move(value)} {}
Value::Value(const char* value) : storage_{value == nullptr ? Storage{} : Storage{std::string{value}}} {}
Value::Value(Blob value) : storage_{std::move(value)} {}

auto Value::is_null() const noexcept -> bool { return std::holds_alternative<std::monostate>(storage_); }
auto Value::as_int() const -> std::int64_t { return std::get<std::int64_t>(storage_); }
auto Value::as_double() const -> double { return std::get<double>(storage_); }
auto Value::as_string() const -> const std::string& { return std::get<std::string>(storage_); }
auto Value::as_blob() const -> std::span<const std::byte> { return std::get<Blob>(storage_); }

} // namespace vulpes::db

