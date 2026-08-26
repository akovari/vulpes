#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"

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

TEST_CASE("inventory seed scenario exercises generic editing and low-stock reporting", "[examples][inventory]") {
    vulpes::db::Database database{":memory:"};
    const std::filesystem::path inventory{VULPES_SOURCE_DIR};
    database.execute(read_script(inventory / "examples" / "inventory" / "schema.sql"));
    database.execute(read_script(inventory / "examples" / "inventory" / "seed.sql"));

    vulpes::model::Dataset products{database, table_schema(database, "products")};
    products.where({"sku", vulpes::model::FilterOperator::equal, "HAM-001"});
    REQUIRE(products.current());
    products.begin_edit();
    products.set("reorder_level", 1);
    products.save();

    vulpes::model::Dataset movements{database, table_schema(database, "stock_movements")};
    movements.begin_insert();
    movements.set("product_id", 1).set("location_id", 1).set("quantity", 4).set("note", "Restocked");
    movements.save();

    auto product = database.prepare("SELECT reorder_level FROM products WHERE id = 1");
    REQUIRE(product.step());
    CHECK(product.column(0).as_int() == 1);

    auto stock = database.prepare("SELECT SUM(quantity) FROM stock_movements WHERE product_id = 1");
    REQUIRE(stock.step());
    CHECK(stock.column(0).as_int() == 6);

    auto report = database.prepare("SELECT count(*) FROM low_stock");
    REQUIRE(report.step());
    CHECK(report.column(0).as_int() == 0);

    vulpes::model::Dataset low_stock{database, table_schema(database, "low_stock")};
    CHECK_FALSE(low_stock.is_editable());
    CHECK(low_stock.rows().empty());
}
