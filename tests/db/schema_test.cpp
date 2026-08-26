#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::db;

TEST_CASE("schema inspection finds fields and relationships", "[db][schema]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    database.execute("CREATE TABLE job(id INTEGER PRIMARY KEY, customer_id INTEGER NOT NULL REFERENCES customer(id))");

    const auto schema = inspect_schema(database);
    REQUIRE(schema.size() == 2);
    const auto& job = schema.at(1);
    CHECK(job.name == "job");
    REQUIRE(job.foreign_keys.size() == 1);
    CHECK(job.foreign_keys.front().field == "customer_id");
    CHECK(job.foreign_keys.front().referenced_table == "customer");
}

