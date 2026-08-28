#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/grid.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {

auto read_script(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input)
        throw std::runtime_error{"unable to open example script: " + path.string()};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto table_schema(vulpes::db::Database& database, std::string_view name) -> vulpes::db::TableSchema {
    const auto schema = vulpes::db::inspect_schema(database);
    const auto table = std::ranges::find(schema, name, &vulpes::db::TableSchema::name);
    if (table == schema.end())
        throw std::runtime_error{"workshop schema is missing table: " + std::string{name}};
    return *table;
}

} // namespace

TEST_CASE("workshop acceptance scenario covers browse edit relationship filter search and SQL",
          "[examples][workshop]") {
    vulpes::db::Database database{":memory:"};
    const std::filesystem::path source{VULPES_SOURCE_DIR};
    database.execute(read_script(source / "examples" / "workshop" / "schema.sql"));
    database.execute(read_script(source / "examples" / "workshop" / "seed.sql"));

    vulpes::model::Dataset customers{database, table_schema(database, "customers")};
    REQUIRE(customers.current());
    CHECK(customers.total_count() == 2);
    customers.begin_insert();
    customers.set("name", "Fox Repairs").set("phone", "+420 555 0300");
    customers.save();

    customers.where({"name", vulpes::model::FilterOperator::equal, "Fox Repairs"});
    REQUIRE(customers.current());
    customers.begin_edit();
    customers.set("phone", "+420 555 0301");
    customers.save();

    vulpes::model::Dataset jobs{database, table_schema(database, "jobs")};
    const auto customer_options = jobs.lookup_options(
        "customer_id", {.display_field = "name", .search_fields = {"name", "phone"}, .search = "Fox", .limit = 20});
    REQUIRE(customer_options.size() == 1);
    CHECK(customer_options.front().label == "Fox Repairs");
    jobs.begin_insert();
    jobs.set("customer_id", customer_options.front().value)
        .set("description", "Repair drill press")
        .set("status", "open");
    jobs.save();

    jobs.where({"status", vulpes::model::FilterOperator::equal, "open"});
    CHECK(jobs.total_count() == 2);
    jobs.search("drill");
    REQUIRE(jobs.current());
    CHECK(jobs.total_count() == 1);
    const auto related_customer = jobs.related_record("customer_id", jobs.current()->at("customer_id"));
    REQUIRE(related_customer);
    CHECK(related_customer->row.at("name").as_string() == "Fox Repairs");

    const auto sql_rows = vulpes::ui::GridRows::from_sql_result(database.run_sql(
        "SELECT customers.name, jobs.description FROM jobs "
        "JOIN customers ON customers.id = jobs.customer_id WHERE jobs.status = 'open' ORDER BY jobs.id"));
    REQUIRE(sql_rows.rows.size() == 2);
    vulpes::ui::Grid sql_grid{sql_rows, "Open jobs", "Esc Back"};
    vulpes::terminal::ScreenBuffer buffer{64, 10};
    sql_grid.render(buffer, {0, 0, 64, 10});
    CHECK(buffer.cell(1, 3).glyph == U'A');
}
