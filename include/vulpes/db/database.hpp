#pragma once

#include "vulpes/db/statement.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace vulpes::db {

enum class OpenMode { read_only, read_write, read_write_create };

// The final tabular result produced by an arbitrary SQL script. Rows own all
// values, so callers never depend on a prepared statement lifetime.
struct SqlResult {
    std::vector<std::string> columns;
    std::vector<Row> rows;
    int changes{};
    bool truncated{false};
};

class Database {
  public:
    explicit Database(const std::filesystem::path& path, OpenMode mode = OpenMode::read_write_create);
    ~Database();

    Database(const Database&) = delete;
    auto operator=(const Database&) -> Database& = delete;
    Database(Database&& other) noexcept;
    auto operator=(Database&& other) noexcept -> Database&;

    [[nodiscard]] auto prepare(std::string_view sql) -> Statement;
    [[nodiscard]] auto run_sql(std::string_view script, std::size_t row_limit = 1'000) -> SqlResult;
    [[nodiscard]] auto run_query(std::string_view query, std::size_t row_limit = 1'000) -> SqlResult;
    void execute(std::string_view sql);
    [[nodiscard]] auto in_transaction() const noexcept -> bool;
    [[nodiscard]] auto is_read_only() const noexcept -> bool { return read_only_; }
    [[nodiscard]] auto changes() const noexcept -> int;
    [[nodiscard]] auto last_insert_rowid() const noexcept -> std::int64_t;

  private:
    friend class Transaction;
    sqlite3* handle_{};
    bool read_only_{false};
};

} // namespace vulpes::db
