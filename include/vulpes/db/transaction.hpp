#pragma once

namespace vulpes::db {

class Database;

class Transaction {
public:
    explicit Transaction(Database& database);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    auto operator=(const Transaction&) -> Transaction& = delete;
    Transaction(Transaction&&) = delete;
    auto operator=(Transaction&&) -> Transaction& = delete;

    void commit();
    void rollback();
    [[nodiscard]] auto active() const noexcept -> bool { return active_; }

private:
    Database* database_;
    bool active_{true};
};

} // namespace vulpes::db

