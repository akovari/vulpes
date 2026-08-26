#include "vulpes/db/statement.hpp"

#include "vulpes/core/error.hpp"

#include "sqlite_error.hpp"

#include <limits>
#include <sqlite3.h>
#include <string>
#include <utility>
#include <vector>

namespace vulpes::db {
namespace {

void check(sqlite3_stmt* statement, int result) {
    if (result != SQLITE_OK) {
        auto* database = sqlite3_db_handle(statement);
        throw detail::sqlite_error(database, result);
    }
}

} // namespace

Statement::Statement(sqlite3_stmt* statement) : statement_{statement} {
}
Statement::~Statement() {
    sqlite3_finalize(statement_);
}
Statement::Statement(Statement&& other) noexcept : statement_{std::exchange(other.statement_, nullptr)} {
}

auto Statement::operator=(Statement&& other) noexcept -> Statement& {
    if (this != &other) {
        sqlite3_finalize(statement_);
        statement_ = std::exchange(other.statement_, nullptr);
    }
    return *this;
}

auto Statement::bind(int index, const Value& value) -> Statement& {
    const int result = std::visit(
        [&](const auto& item) -> int {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return sqlite3_bind_null(statement_, index);
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return sqlite3_bind_int64(statement_, index, item);
            else if constexpr (std::is_same_v<T, double>)
                return sqlite3_bind_double(statement_, index, item);
            else if constexpr (std::is_same_v<T, std::string>)
                return sqlite3_bind_text64(statement_, index, item.data(), item.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
            else if (item.empty())
                return sqlite3_bind_zeroblob64(statement_, index, 0);
            else
                return sqlite3_bind_blob64(statement_, index, item.data(), item.size(), SQLITE_TRANSIENT);
        },
        value.storage());
    check(statement_, result);
    return *this;
}

auto Statement::bind(std::string_view name, const Value& value) -> Statement& {
    const std::string owned_name{name};
    const int index = sqlite3_bind_parameter_index(statement_, owned_name.c_str());
    if (index == 0)
        throw Error{ErrorCategory::database, "unknown SQL parameter: " + owned_name};
    return bind(index, value);
}

auto Statement::step() -> bool {
    const int result = sqlite3_step(statement_);
    if (result == SQLITE_ROW)
        return true;
    if (result == SQLITE_DONE)
        return false;
    auto* database = sqlite3_db_handle(statement_);
    throw detail::sqlite_error(database, result);
}

void Statement::execute() {
    while (step()) {
    }
}

void Statement::reset() {
    check(statement_, sqlite3_reset(statement_));
    check(statement_, sqlite3_clear_bindings(statement_));
}

auto Statement::column_count() const -> int {
    return sqlite3_column_count(statement_);
}

auto Statement::column_name(int index) const -> std::string_view {
    const char* name = sqlite3_column_name(statement_, index);
    return name ? std::string_view{name} : std::string_view{};
}

auto Statement::column(int index) const -> Value {
    switch (sqlite3_column_type(statement_, index)) {
    case SQLITE_NULL:
        return {};
    case SQLITE_INTEGER:
        return Value{sqlite3_column_int64(statement_, index)};
    case SQLITE_FLOAT:
        return Value{sqlite3_column_double(statement_, index)};
    case SQLITE_TEXT: {
        const auto* data = reinterpret_cast<const char*>(sqlite3_column_text(statement_, index));
        const auto size = static_cast<std::size_t>(sqlite3_column_bytes(statement_, index));
        if (size == 0)
            return Value{std::string{}};
        return Value{std::string{data, size}};
    }
    case SQLITE_BLOB: {
        const auto* data = static_cast<const std::byte*>(sqlite3_column_blob(statement_, index));
        const auto size = static_cast<std::size_t>(sqlite3_column_bytes(statement_, index));
        if (size == 0)
            return Value{Blob{}};
        return Value{Blob{data, data + size}};
    }
    default:
        throw Error{ErrorCategory::database, "unsupported SQLite value type"};
    }
}

auto Statement::row() const -> Row {
    std::vector<std::string> names;
    std::vector<Value> values;
    names.reserve(static_cast<std::size_t>(column_count()));
    values.reserve(static_cast<std::size_t>(column_count()));
    for (int index = 0; index < column_count(); ++index) {
        names.emplace_back(column_name(index));
        values.push_back(column(index));
    }
    return Row{std::move(names), std::move(values)};
}

} // namespace vulpes::db
