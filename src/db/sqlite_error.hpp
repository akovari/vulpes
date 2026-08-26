#pragma once

#include "vulpes/core/error.hpp"

#include <sqlite3.h>
#include <string>

namespace vulpes::db::detail {

[[nodiscard]] inline auto sqlite_error(sqlite3* database, int code, std::string message = {}) -> Error {
    if (message.empty() && database != nullptr)
        message = sqlite3_errmsg(database);
    if (message.empty())
        message = "SQLite operation failed";

    ErrorCategory category = ErrorCategory::database;
    switch (code & 0xFF) {
    case SQLITE_CONSTRAINT:
        category = ErrorCategory::constraint;
        break;
    case SQLITE_IOERR:
    case SQLITE_CANTOPEN:
    case SQLITE_READONLY:
        category = ErrorCategory::io;
        break;
    default:
        break;
    }
    return Error{category, std::move(message), code};
}

} // namespace vulpes::db::detail
