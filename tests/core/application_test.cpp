#include "vulpes/appmeta/loader.hpp"
#include "vulpes/core/application.hpp"
#include "vulpes/core/command.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/script/runtime.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string_view>

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
    const auto export_report = application.execute(core::parse_command("export names PDF output.pdf overwrite"));
    REQUIRE(export_report.outcome == core::CommandOutcome::export_report);
    REQUIRE(export_report.report);
    REQUIRE(export_report.export_format);
    CHECK(*export_report.export_format == report::ExportFormat::pdf);
    CHECK(export_report.export_destination == "output.pdf");
    CHECK(export_report.export_overwrite);
    CHECK(application.execute(core::parse_command("export names xml output.xml")).outcome ==
          core::CommandOutcome::invalid_arguments);
    CHECK(application.execute(core::parse_command("export names csv output.csv replace")).outcome ==
          core::CommandOutcome::invalid_arguments);
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
    CHECK(application.execute(core::parse_command("export absent csv result.csv")).outcome ==
          core::CommandOutcome::definition_not_found);
    CHECK(application.execute(core::parse_command("export report xml result.xml")).outcome ==
          core::CommandOutcome::definition_not_found);
}

TEST_CASE("application runtime invokes Lua command hooks before semantic dispatch", "[core][application][script]") {
    db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY)");
    script::Runtime scripts{{{.name = "guard-browse",
                              .hook = script::Hook::on_command,
                              .command = "browse",
                              .source = "error('browse is closed for maintenance')"}}};
    core::ApplicationRuntime application{database, nullptr, &scripts};

    CHECK(application.execute(core::parse_command("tables")).outcome == core::CommandOutcome::tables);
    try {
        static_cast<void>(application.execute(core::parse_command("browse customer")));
        FAIL("the command hook should prevent dispatch");
    } catch (const Error& error) {
        CHECK(error.category() == ErrorCategory::script);
        CHECK(std::string_view{error.what()}.contains("maintenance"));
    }
}
