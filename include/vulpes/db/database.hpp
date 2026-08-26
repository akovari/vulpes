#pragma once

#include "vulpes/db/statement.hpp"

#include <cstdint>
#include <filesystem>
#include <string_view>

struct sqlite3;

namespace vulpes::db {

enum class OpenMode { read_only, read_write, read_write_create };

class Database {
public:
    explicit Database(const std::filesystem::path& path, OpenMode mode = OpenMode::read_write_create);
    ~Database();

    Database(const Database&) = delete;
    auto operator=(const Database&) -> Database& = delete;
    Database(Database&& other) noexcept;
    auto operator=(Database&& other) noexcept -> Database&;

    [[nodiscard]] auto prepare(std::string_view sql) -> Statement;
    void execute(std::string_view sql);
    [[nodiscard]] auto changes() const noexcept -> int;
    [[nodiscard]] auto last_insert_rowid() const noexcept -> std::int64_t;

private:
    friend class Transaction;
    sqlite3* handle_{};
};

} // namespace vulpes::db
