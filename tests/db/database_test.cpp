#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/transaction.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::db;

TEST_CASE("values round-trip through SQLite", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY, name TEXT, score REAL, payload BLOB)");

    auto insert = database.prepare("INSERT INTO sample(name, score) VALUES (?, ?)");
    insert.bind(1, "Liška").bind(2, 4.5).execute();

    auto query = database.prepare("SELECT id, name, score, payload FROM sample");
    REQUIRE(query.step());
    CHECK(query.column(0).as_int() == 1);
    CHECK(query.column(1).as_string() == "Liška");
    CHECK(query.column(2).as_double() == 4.5);
    CHECK(query.column(3).is_null());
    CHECK_FALSE(query.step());
}

TEST_CASE("an uncommitted transaction rolls back", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(value INTEGER)");
    {
        Transaction transaction{database};
        database.execute("INSERT INTO sample VALUES (42)");
    }
    auto count = database.prepare("SELECT count(*) FROM sample");
    REQUIRE(count.step());
    CHECK(count.column(0).as_int() == 0);
}

TEST_CASE("execute accepts a complete SQL script", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE first(value TEXT);"
                     "CREATE TABLE second(value TEXT);"
                     "INSERT INTO second VALUES ('ready');");

    auto query = database.prepare("SELECT value FROM second");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "ready");
}

TEST_CASE("rows own values independently from their statement", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(value TEXT); INSERT INTO sample VALUES ('persist');");
    auto query = database.prepare("SELECT value FROM sample");
    REQUIRE(query.step());
    const auto row = query.row();
    query.reset();
    CHECK(row.size() == 1);
    CHECK(row.column_name(0) == "value");
    CHECK(row.at("value").as_string() == "persist");
    CHECK_THROWS_AS(row.at("missing"), std::out_of_range);
}

TEST_CASE("nested transactions fail before SQLite execution", "[db]") {
    Database database{":memory:"};
    Transaction outer{database};
    CHECK(database.in_transaction());
    CHECK_THROWS_AS(Transaction{database}, vulpes::Error);
    outer.rollback();
    CHECK_FALSE(database.in_transaction());
}

TEST_CASE("constraint failures retain SQLite result codes", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(value TEXT UNIQUE); INSERT INTO sample VALUES ('one');");
    try {
        database.execute("INSERT INTO sample VALUES ('one')");
        FAIL("expected a unique constraint violation");
    } catch (const vulpes::Error& error) {
        CHECK(error.category() == vulpes::ErrorCategory::constraint);
        CHECK(error.native_code() != 0);
    }
}
