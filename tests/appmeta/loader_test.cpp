#include "vulpes/appmeta/loader.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ordinary SQLite databases load no application definition and are not modified", "[appmeta][loader]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT)");

    CHECK_FALSE(vulpes::appmeta::has_application_metadata(database));
    CHECK(vulpes::appmeta::load_application_definition(database).empty());
    const auto schema = vulpes::db::inspect_schema(database);
    CHECK(std::ranges::none_of(
        schema, [](const auto& table) { return vulpes::appmeta::is_application_metadata_table(table.name); }));
}

TEST_CASE("application metadata migrations are transactional versioned and idempotent", "[appmeta][migration]") {
    vulpes::db::Database database{":memory:"};

    vulpes::appmeta::migrate_application_metadata(database);
    REQUIRE(vulpes::appmeta::application_schema_version(database));
    CHECK(*vulpes::appmeta::application_schema_version(database) ==
          vulpes::appmeta::current_application_schema_version);
    CHECK_NOTHROW(vulpes::appmeta::migrate_application_metadata(database));

    database.execute("INSERT INTO _app_settings(key, value) VALUES('title', 'Workshop');DROP TABLE _app_scripts;"
                     "DROP TABLE _app_menu_items; DROP TABLE _app_menus; DROP TABLE _app_commands;"
                     "DROP TABLE _app_reports; DROP TABLE _app_views;"
                     "UPDATE _app_schema SET version = 1 WHERE singleton = 1;");
    vulpes::appmeta::migrate_application_metadata(database);

    CHECK(*vulpes::appmeta::application_schema_version(database) == 3);
    auto setting = database.prepare("SELECT value FROM _app_settings WHERE key = 'title'");
    REQUIRE(setting.step());
    CHECK(setting.column(0).as_string() == "Workshop");
}

TEST_CASE("loader builds and validates complete SQLite resident application metadata", "[appmeta][loader]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE supplier(id INTEGER PRIMARY KEY, name TEXT NOT NULL, code TEXT);"
                     "CREATE TABLE product(id INTEGER PRIMARY KEY, supplier_id INTEGER REFERENCES supplier(id), "
                     "name TEXT NOT NULL, price REAL, active INTEGER);"
                     "CREATE VIEW active_products AS SELECT * FROM product WHERE active = 1;");
    vulpes::appmeta::migrate_application_metadata(database);
    database.execute("INSERT INTO _app_settings VALUES('title', 'Inventory');"
                     "INSERT INTO _app_settings VALUES('start_command', 'run products');"
                     "INSERT INTO _app_forms VALUES('product', 'product', 'Product', 1);"
                     "INSERT INTO _app_form_fields(form_name, field_name, label, position, visible, read_only, format, "
                     "currency_code) VALUES('product', 'price', 'Price', 2, 1, 0, 'currency', 'EUR');"
                     "INSERT INTO _app_form_fields(form_name, field_name, label, position, lookup_display_field, "
                     "lookup_search_fields, lookup_result_limit, lookup_allow_drill_down) "
                     "VALUES('product', 'supplier_id', 'Supplier', 1, 'name', '[\"name\",\"code\"]', 25, 1);"
                     "INSERT INTO _app_views VALUES('active', 'active_products', 'Active products', NULL);"
                     "INSERT INTO _app_reports VALUES('prices', 'Product prices', "
                     "'SELECT name, price FROM product ORDER BY name', 500);"
                     "INSERT INTO _app_commands VALUES('products', 'Products', 'browse product');"
                     "INSERT INTO _app_commands VALUES('prices', 'Prices', 'report prices');"
                     "INSERT INTO _app_menus VALUES('main', 'Main', 0);"
                     "INSERT INTO _app_menu_items VALUES('main', 0, 'Products', 'products');"
                     "INSERT INTO _app_menu_items VALUES('main', 1, 'Prices', 'prices');"
                     "INSERT INTO _app_scripts VALUES('normalize-product', 'before_insert', 'product', NULL, "
                     "'record.name = string.upper(record.name)', 0);");

    const auto definition = vulpes::appmeta::load_application_definition(database);

    CHECK_FALSE(definition.empty());
    CHECK(definition.setting("title") == "Inventory");
    REQUIRE(definition.default_form("product") != nullptr);
    CHECK(definition.default_form("product")->presentation.field("price")->currency_code == "EUR");
    REQUIRE(definition.default_form("product")->presentation.field("supplier_id")->lookup);
    CHECK(definition.default_form("product")->presentation.field("supplier_id")->lookup->search_fields ==
          std::vector<std::string>{"name", "code"});
    REQUIRE(definition.view("active") != nullptr);
    REQUIRE(definition.command("products") != nullptr);
    REQUIRE(definition.report("prices") != nullptr);
    REQUIRE(definition.menus.size() == 1);
    CHECK(definition.menus.front().items.size() == 2);
    REQUIRE(definition.scripts.size() == 1);
    CHECK(definition.scripts.front().table == "product");
    REQUIRE(definition.presentation.table("product") != nullptr);
}

TEST_CASE("loader rejects future versions and metadata that contradicts SQLite schema", "[appmeta][loader]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE product(id INTEGER PRIMARY KEY, name TEXT)");
    vulpes::appmeta::migrate_application_metadata(database);
    database.execute("INSERT INTO _app_forms VALUES('product', 'product', 'Product', 1);"
                     "INSERT INTO _app_form_fields(form_name, field_name) VALUES('product', 'missing');");
    CHECK_THROWS_AS(vulpes::appmeta::load_application_definition(database), vulpes::Error);

    database.execute("DELETE FROM _app_form_fields; UPDATE _app_schema SET version = 4 WHERE singleton = 1;");
    CHECK_THROWS_AS(vulpes::appmeta::load_application_definition(database), vulpes::Error);
    CHECK_THROWS_AS(vulpes::appmeta::migrate_application_metadata(database), vulpes::Error);
}

TEST_CASE("loader rejects invalid Lua script scope and source metadata", "[appmeta][script]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE product(id INTEGER PRIMARY KEY, name TEXT)");
    vulpes::appmeta::migrate_application_metadata(database);
    database.execute("INSERT INTO _app_scripts VALUES('wrong-scope', 'before_insert', 'missing', NULL, 'return', 0)");
    CHECK_THROWS_AS(vulpes::appmeta::load_application_definition(database), vulpes::Error);

    database.execute("DELETE FROM _app_scripts;"
                     "INSERT INTO _app_scripts VALUES('wrong-command', 'on_command', NULL, 'Bad Name', 'return', 0)");
    CHECK_THROWS_AS(vulpes::appmeta::load_application_definition(database), vulpes::Error);
}
