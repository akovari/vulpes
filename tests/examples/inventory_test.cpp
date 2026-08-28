#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/grid.hpp"
#include "vulpes/ui/relationship_lookup.hpp"

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
        throw std::runtime_error{"example schema is missing table or view: " + std::string{name}};
    return *table;
}

} // namespace

TEST_CASE("inventory acceptance scenario exercises generic edit relationship movement and report workflows",
          "[examples][inventory]") {
    vulpes::db::Database database{":memory:"};
    const std::filesystem::path inventory{VULPES_SOURCE_DIR};
    database.execute(read_script(inventory / "examples" / "inventory" / "schema.sql"));
    database.execute(read_script(inventory / "examples" / "inventory" / "seed.sql"));

    const auto low_stock_result =
        database.run_sql("SELECT sku, name, quantity, reorder_level FROM low_stock ORDER BY sku");
    REQUIRE(low_stock_result.rows.size() == 1);
    CHECK(low_stock_result.rows.front().at("sku").as_string() == "HAM-001");
    const auto low_stock_rows = vulpes::ui::GridRows::from_sql_result(low_stock_result);
    vulpes::ui::Grid low_stock_grid{low_stock_rows, "Low stock", "Esc Back"};
    vulpes::terminal::ScreenBuffer report_buffer{64, 10};
    low_stock_grid.render(report_buffer, {0, 0, 64, 10});
    CHECK(report_buffer.cell(1, 3).glyph == U'H');

    vulpes::model::Dataset products{database, table_schema(database, "products")};
    products.where({"sku", vulpes::model::FilterOperator::equal, "HAM-001"});
    REQUIRE(products.current());

    vulpes::ui::RelationshipLookup category_lookup{products,
                                                   "category_id",
                                                   {.display_field = "name", .search_fields = {"name"}, .limit = 20},
                                                   true,
                                                   "Category",
                                                   "Search:",
                                                   "Enter Select  F2 View  Esc Cancel"};
    for (const auto character : std::u32string{U"Tools"})
        static_cast<void>(category_lookup.handle(
            vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = character}));
    REQUIRE(category_lookup.options().size() == 1);
    CHECK(category_lookup.selected_option()->label == "Tools");
    CHECK(category_lookup.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f2}) ==
          vulpes::ui::RelationshipLookupResult::drill_down);
    const auto category = products.related_record("category_id", category_lookup.selected_option()->value);
    REQUIRE(category);
    vulpes::ui::RelatedRecordView category_view{*category, "Category", "Esc Back"};
    vulpes::terminal::ScreenBuffer category_buffer{48, 8};
    category_view.render(category_buffer, {0, 0, 48, 8});
    CHECK(category_view.field_count() == 2);

    products.begin_edit();
    products.set("reorder_level", 1);
    products.save();

    vulpes::model::Dataset movements{database, table_schema(database, "stock_movements")};
    const auto product_options = movements.lookup_options(
        "product_id", {.display_field = "name", .search_fields = {"sku", "name"}, .search = "hammer", .limit = 20});
    const auto location_options = movements.lookup_options(
        "location_id", {.display_field = "code", .search_fields = {"code", "name"}, .search = "MAIN", .limit = 20});
    REQUIRE(product_options.size() == 1);
    REQUIRE(location_options.size() == 1);
    movements.begin_insert();
    movements.set("product_id", product_options.front().value)
        .set("location_id", location_options.front().value)
        .set("quantity", 4)
        .set("note", "Restocked");
    movements.save();

    auto product = database.prepare("SELECT reorder_level FROM products WHERE id = 1");
    REQUIRE(product.step());
    CHECK(product.column(0).as_int() == 1);

    auto stock = database.prepare("SELECT SUM(quantity) FROM stock_movements WHERE product_id = 1");
    REQUIRE(stock.step());
    CHECK(stock.column(0).as_int() == 6);

    auto movement = database.prepare("SELECT occurred_at FROM stock_movements WHERE note = 'Restocked'");
    REQUIRE(movement.step());
    CHECK(movement.column(0).as_string().contains('T'));
    CHECK(movement.column(0).as_string().ends_with('Z'));

    auto report = database.prepare("SELECT count(*) FROM low_stock");
    REQUIRE(report.step());
    CHECK(report.column(0).as_int() == 0);

    vulpes::model::Dataset low_stock{database, table_schema(database, "low_stock")};
    CHECK_FALSE(low_stock.is_editable());
    CHECK(low_stock.rows().empty());
}
