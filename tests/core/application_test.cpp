#include "vulpes/appmeta/loader.hpp"
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

    CHECK(application.execute(core::parse_command("sql")).outcome == core::CommandOutcome::sql);
}

TEST_CASE("application runtime resolves metadata forms views reports and commands", "[core][application][appmeta]") {
    db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT);"
                     "CREATE VIEW active_customers AS SELECT * FROM customer;");
    appmeta::migrate_application_metadata(database);
    database.execute("INSERT INTO _app_forms VALUES('customer', 'customer', 'Customer', 1);"
                     "INSERT INTO _app_views VALUES('active', 'active_customers', 'Active customers', NULL);"
                     "INSERT INTO _app_reports VALUES('names', 'Customer names', 'SELECT name FROM customer', 100);"
                     "INSERT INTO _app_commands VALUES('dashboard', 'Dashboard', 'report names');");
    const auto definition = appmeta::load_application_definition(database);
    core::ApplicationRuntime application{database, &definition};

    const auto tables = application.execute(core::parse_command("tables"));
    CHECK(tables.tables.size() == 2);
    CHECK(std::ranges::none_of(tables.tables, [](const auto& table) { return table.name.starts_with("_app_"); }));
    CHECK(application.execute(core::parse_command("forms")).forms.size() == 1);
    CHECK(application.execute(core::parse_command("views")).views.size() == 1);
    CHECK(application.execute(core::parse_command("reports")).reports.size() == 1);

    const auto form = application.execute(core::parse_command("form customer"));
    REQUIRE(form.outcome == core::CommandOutcome::browse);
    REQUIRE(form.form);
    CHECK(form.table->name == "customer");

    const auto view = application.execute(core::parse_command("view active"));
    REQUIRE(view.outcome == core::CommandOutcome::browse);
    REQUIRE(view.view);
    CHECK(view.table->name == "active_customers");

    const auto report = application.execute(core::parse_command("run dashboard"));
    REQUIRE(report.outcome == core::CommandOutcome::report);
    REQUIRE(report.report);
    CHECK(report.report->name == "names");
    CHECK(application.execute(core::parse_command("dashboard")).outcome == core::CommandOutcome::report);
    CHECK(application.execute(core::parse_command("report absent")).outcome ==
          core::CommandOutcome::definition_not_found);
    CHECK(application.execute(core::parse_command("browse _app_forms")).outcome ==
          core::CommandOutcome::table_not_found);
}

TEST_CASE("application runtime bounds recursive metadata commands", "[core][application][appmeta]") {
    db::Database database{":memory:"};
    appmeta::migrate_application_metadata(database);
    database.execute("INSERT INTO _app_commands VALUES('loop', 'Loop', 'run loop')");
    const auto definition = appmeta::load_application_definition(database);
    core::ApplicationRuntime application{database, &definition};

    CHECK(application.execute(core::parse_command("run loop")).outcome == core::CommandOutcome::command_cycle);
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
    CHECK(application.execute(core::parse_command("sql extra")).outcome == core::CommandOutcome::invalid_arguments);
}
