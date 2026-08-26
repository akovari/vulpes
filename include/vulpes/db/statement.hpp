#pragma once

#include "vulpes/db/value.hpp"

#include <cstddef>
#include <memory>
#include <string_view>

struct sqlite3_stmt;

namespace vulpes::db {

class Statement {
public:
    Statement() noexcept = default;
    explicit Statement(sqlite3_stmt* statement);
    ~Statement();

    Statement(const Statement&) = delete;
    auto operator=(const Statement&) -> Statement& = delete;
    Statement(Statement&& other) noexcept;
    auto operator=(Statement&& other) noexcept -> Statement&;

    auto bind(int index, const Value& value) -> Statement&;
    auto bind(std::string_view name, const Value& value) -> Statement&;
    [[nodiscard]] auto step() -> bool;
    void execute();
    void reset();

    [[nodiscard]] auto column_count() const -> int;
    [[nodiscard]] auto column_name(int index) const -> std::string_view;
    [[nodiscard]] auto column(int index) const -> Value;

private:
    sqlite3_stmt* statement_{};
};

} // namespace vulpes::db

