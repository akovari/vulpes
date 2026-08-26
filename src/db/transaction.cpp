#include "vulpes/db/transaction.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"

namespace vulpes::db {

Transaction::Transaction(Database& database) : database_{&database} {
    if (database_->in_transaction())
        throw Error{ErrorCategory::database, "nested transactions are not supported"};
    database_->execute("BEGIN IMMEDIATE");
}

Transaction::~Transaction() {
    if (active_) {
        try {
            database_->execute("ROLLBACK");
        } catch (...) {
        }
    }
}

void Transaction::commit() {
    if (!active_)
        return;
    database_->execute("COMMIT");
    active_ = false;
}

void Transaction::rollback() {
    if (!active_)
        return;
    database_->execute("ROLLBACK");
    active_ = false;
}

} // namespace vulpes::db
