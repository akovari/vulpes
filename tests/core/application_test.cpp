#include "vulpes/core/application.hpp"
#include "vulpes/core/command.hpp"
#include "vulpes/db/database.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes;

TEST_CASE("application runtime dispatches database commands without frontend dependencies", "[core][application]") {
    db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
                     "CREATE VIEW customer_names AS SELECT name FROM customer;");
    core::ApplicationRuntime application{database};

    const auto tables = application.execute(core::parse_command("tables"));
    REQUIRE(tables.outcome == core::CommandOutcome::tables);
    REQUIRE(tables.tables.size() == 2);

    const auto schema = application.execute(core::parse_command("schema customer"));
    REQUIRE(schema.outcome == core::CommandOutcome::schema);
    REQUIRE(schema.table);
    CHECK(schema.table->name == "customer");
    CHECK(schema.table->fields.size() == 2);

    const auto browse = application.execute(core::parse_command("browse customer_names"));
    REQUIRE(browse.outcome == core::CommandOutcome::browse);
    REQUIRE(browse.table);
    CHECK(browse.table->is_view);
}

TEST_CASE("application runtime returns semantic command failures", "[core][application]") {
    db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY)");
    core::ApplicationRuntime application{database};

    CHECK(application.execute(core::parse_command("invented")).outcome == core::CommandOutcome::unknown_command);
    CHECK(application.execute(core::parse_command("tables extra")).outcome == core::CommandOutcome::invalid_arguments);
    CHECK(application.execute(core::parse_command("schema")).outcome == core::CommandOutcome::invalid_arguments);
    CHECK(application.execute(core::parse_command("browse absent")).outcome == core::CommandOutcome::table_not_found);
    CHECK(application.execute(core::parse_command("quit now")).outcome == core::CommandOutcome::invalid_arguments);
}
