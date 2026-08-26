#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::db;

TEST_CASE("schema inspection finds fields and relationships", "[db][schema]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)");
    database.execute("CREATE TABLE job(id INTEGER PRIMARY KEY, customer_id INTEGER NOT NULL REFERENCES customer(id) ON UPDATE CASCADE ON DELETE RESTRICT)");

    const auto schema = inspect_schema(database);
    REQUIRE(schema.size() == 2);
    const auto& job = schema.at(1);
    CHECK(job.name == "job");
    REQUIRE(job.foreign_keys.size() == 1);
    CHECK(job.foreign_keys.front().field == "customer_id");
    CHECK(job.foreign_keys.front().referenced_table == "customer");
    CHECK(job.foreign_keys.front().on_update == "CASCADE");
    CHECK(job.foreign_keys.front().on_delete == "RESTRICT");

    const auto& customer = schema.at(0);
    REQUIRE(customer.indexes.size() == 1);
    CHECK(customer.indexes.front().unique);
    CHECK(customer.fields.at(1).unique);
    CHECK(customer.primary_key_fields() == std::vector<std::string>{"id"});
}

TEST_CASE("schema preserves composite primary-key order", "[db][schema]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(second INTEGER, first INTEGER, PRIMARY KEY(first, second))");

    const auto schema = inspect_schema(database);
    REQUIRE(schema.size() == 1);
    CHECK(schema.front().primary_key_fields() == std::vector<std::string>{"first", "second"});
}
