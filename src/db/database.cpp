#include "vulpes/db/database.hpp"

#include "vulpes/core/error.hpp"

#include "sqlite_error.hpp"

#include <limits>
#include <sqlite3.h>
#include <utility>

namespace vulpes::db {
namespace {

auto flags_for(OpenMode mode) -> int {
    switch (mode) {
    case OpenMode::read_only:
        return SQLITE_OPEN_READONLY;
    case OpenMode::read_write:
        return SQLITE_OPEN_READWRITE;
    case OpenMode::read_write_create:
        return SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    }
    return SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
}

} // namespace

Database::Database(const std::filesystem::path& path, OpenMode mode) {
    const auto utf8_path = path.u8string();
    const auto* filename = reinterpret_cast<const char*>(utf8_path.c_str());
    const int result = sqlite3_open_v2(filename, &handle_, flags_for(mode), nullptr);
    if (result != SQLITE_OK) {
        const std::string message = handle_ ? sqlite3_errmsg(handle_) : "unable to allocate SQLite connection";
        sqlite3_close(handle_);
        handle_ = nullptr;
        throw detail::sqlite_error(nullptr, result, message);
    }
    sqlite3_extended_result_codes(handle_, 1);
    sqlite3_busy_timeout(handle_, 5'000);
    execute("PRAGMA foreign_keys = ON");
}

Database::~Database() {
    sqlite3_close(handle_);
}

Database::Database(Database&& other) noexcept : handle_{std::exchange(other.handle_, nullptr)} {
}

auto Database::operator=(Database&& other) noexcept -> Database& {
    if (this != &other) {
        sqlite3_close(handle_);
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

auto Database::prepare(std::string_view sql) -> Statement {
    sqlite3_stmt* statement{};
    const int result = sqlite3_prepare_v2(handle_, sql.data(), static_cast<int>(sql.size()), &statement, nullptr);
    if (result != SQLITE_OK) {
        throw detail::sqlite_error(handle_, result);
    }
    return Statement{statement};
}

auto Database::run_sql(std::string_view script, std::size_t row_limit) -> SqlResult {
    if (row_limit == 0)
        throw Error{ErrorCategory::validation, "SQL result row limit must be greater than zero"};
    if (script.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw Error{ErrorCategory::database, "SQL script exceeds SQLite's maximum statement size"};

    SqlResult result;
    const auto* current = script.data();
    const auto* const end = script.data() + script.size();
    while (current < end) {
        sqlite3_stmt* raw_statement{};
        const char* tail{};
        const int prepare_result =
            sqlite3_prepare_v2(handle_, current, static_cast<int>(end - current), &raw_statement, &tail);
        if (prepare_result != SQLITE_OK)
            throw detail::sqlite_error(handle_, prepare_result);
        if (tail == nullptr || tail <= current)
            break;
        current = tail;
        if (raw_statement == nullptr)
            continue;

        Statement statement{raw_statement};
        if (statement.column_count() == 0) {
            statement.execute();
            result.changes += changes();
            continue;
        }

        result.columns.clear();
        result.rows.clear();
        result.truncated = false;
        for (int index = 0; index < statement.column_count(); ++index)
            result.columns.emplace_back(statement.column_name(index));
        while (statement.step()) {
            if (result.rows.size() == row_limit) {
                result.truncated = true;
                break;
            }
            result.rows.push_back(statement.row());
        }
    }
    return result;
}

void Database::execute(std::string_view sql) {
    const std::string script{sql};
    char* error_message{};
    const int result = sqlite3_exec(handle_, script.c_str(), nullptr, nullptr, &error_message);
    if (result != SQLITE_OK) {
        const std::string message = error_message ? error_message : sqlite3_errmsg(handle_);
        sqlite3_free(error_message);
        throw detail::sqlite_error(handle_, result, message);
    }
}

auto Database::changes() const noexcept -> int {
    return sqlite3_changes(handle_);
}
auto Database::last_insert_rowid() const noexcept -> std::int64_t {
    return sqlite3_last_insert_rowid(handle_);
}
auto Database::in_transaction() const noexcept -> bool {
    return sqlite3_get_autocommit(handle_) == 0;
}

} // namespace vulpes::db
