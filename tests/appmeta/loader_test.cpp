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
                     "DROP TABLE _app_reports; DROP TABLE _app_view_filter_terms; DROP TABLE _app_view_filters;"
                     "DROP TABLE _app_views; DROP TABLE _app_screen_items;"
                     "DROP TABLE _app_screens; UPDATE _app_schema SET version = 1 WHERE singleton = 1;");
    vulpes::appmeta::migrate_application_metadata(database);

    CHECK(*vulpes::appmeta::application_schema_version(database) == 5);
    const auto schema = vulpes::db::inspect_schema(database);
    const auto screen_table = std::ranges::find(schema, "_app_screens", &vulpes::db::TableSchema::name);
    REQUIRE(screen_table != schema.end());
    CHECK(std::ranges::none_of(screen_table->fields, [](const auto& field) {
        return field.name == "x" || field.name == "y" || field.name == "width" || field.name == "height";
    }));
    auto setting = database.prepare("SELECT value FROM _app_settings WHERE key = 'title'");
    REQUIRE(setting.step());
    CHECK(setting.column(0).as_string() == "Workshop");

    vulpes::db::Database version_four{":memory:"};
    version_four.execute("CREATE TABLE _app_schema("
                         "singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
                         "version INTEGER NOT NULL CHECK(version > 0));"
                         "INSERT INTO _app_schema(singleton, version) VALUES(1, 4);"
                         "CREATE TABLE _app_views("
                         "name TEXT PRIMARY KEY, table_name TEXT NOT NULL, label TEXT,"
                         "form_name TEXT);");
    vulpes::appmeta::migrate_application_metadata(version_four);
    const auto upgraded_schema = vulpes::db::inspect_schema(version_four);
    const auto view_table = std::ranges::find(upgraded_schema, "_app_views", &vulpes::db::TableSchema::name);
    REQUIRE(view_table != upgraded_schema.end());
    CHECK(
        std::ranges::any_of(view_table->fields, [](const auto& field) { return field.name == "default_filter_name"; }));
    CHECK(
        std::ranges::any_of(upgraded_schema, [](const auto& table) { return table.name == "_app_view_filter_terms"; }));
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
                     "INSERT INTO _app_views(name, table_name, label, form_name) "
                     "VALUES('active', 'active_products', 'Active products', NULL);"
                     "INSERT INTO _app_reports VALUES('prices', 'Product prices', "
                     "'SELECT name, price FROM product ORDER BY name', 500);"
                     "INSERT INTO _app_commands VALUES('products', 'Products', 'browse product');"
                     "INSERT INTO _app_commands VALUES('prices', 'Prices', 'report prices');"
                     "INSERT INTO _app_screens VALUES('home', 'Inventory home', 'Choose an inventory task.', 1);"
                     "INSERT INTO _app_screen_items VALUES('home', 0, 'Products', 'Browse the product catalogue.', "
                     "'products');"
                     "INSERT INTO _app_screen_items VALUES('home', 1, 'Product prices', NULL, 'prices');"
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
    REQUIRE(definition.screen("home") != nullptr);
    CHECK(definition.default_screen() == definition.screen("home"));
    CHECK(definition.screen("home")->items.size() == 2);
    CHECK(definition.screen("home")->items.front().command == "products");
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

    database.execute("DELETE FROM _app_form_fields; UPDATE _app_schema SET version = 6 WHERE singleton = 1;");
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

TEST_CASE("loader rejects invalid semantic application screen metadata", "[appmeta][screen]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE product(id INTEGER PRIMARY KEY, name TEXT)");
    vulpes::appmeta::migrate_application_metadata(database);
    database.execute("INSERT INTO _app_commands VALUES('products', 'Products', 'browse product');"
                     "INSERT INTO _app_screens VALUES('home', '', NULL, 1);");

    CHECK_THROWS_AS(vulpes::appmeta::load_application_definition(database), vulpes::Error);
}

TEST_CASE("loader retains typed named view filters and validates their semantic references",
          "[appmeta][view][filters]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE product(id INTEGER PRIMARY KEY, name TEXT, price REAL, photo BLOB);"
                     "CREATE VIEW visible_product AS SELECT * FROM product;");
    vulpes::appmeta::migrate_application_metadata(database);
    database.execute(
        "INSERT INTO _app_views(name, table_name, label, default_filter_name) "
        "VALUES('visible', 'visible_product', 'Visible products', 'priced');"
        "INSERT INTO _app_view_filters(view_name, name, position, search_text) "
        "VALUES('visible', 'priced', 0, 'widget');"
        "INSERT INTO _app_view_filter_terms(view_name, filter_name, position, field_name, comparison, value_kind, "
        "integer_value) VALUES('visible', 'priced', 0, 'id', 'greater', 'integer', 5);"
        "INSERT INTO _app_view_filter_terms(view_name, filter_name, position, field_name, comparison, value_kind, "
        "real_value) VALUES('visible', 'priced', 1, 'price', 'less_equal', 'real', 12.5);"
        "INSERT INTO _app_view_filter_terms(view_name, filter_name, position, field_name, comparison, value_kind, "
        "text_value) VALUES('visible', 'priced', 2, 'name', 'not_equal', 'text', 'retired');"
        "INSERT INTO _app_view_filter_terms(view_name, filter_name, position, field_name, comparison, value_kind, "
        "blob_value) VALUES('visible', 'priced', 3, 'photo', 'equal', 'blob', X'CAFE');");

    const auto definition = vulpes::appmeta::load_application_definition(database);
    const auto* view = definition.view("visible");
    REQUIRE(view != nullptr);
    REQUIRE(view->default_filter);
    CHECK(*view->default_filter == "priced");
    REQUIRE(view->filters.size() == 1);
    const auto& filter = view->filters.front();
    CHECK(filter.name == "priced");
    CHECK(filter.search == "widget");
    REQUIRE(filter.filters.size() == 4);
    CHECK(filter.filters[0].comparison == vulpes::model::FilterOperator::greater);
    CHECK(filter.filters[0].value.as_int() == 5);
    CHECK(filter.filters[1].value.as_double() == 12.5);
    CHECK(filter.filters[2].value.as_string() == "retired");
    CHECK(filter.filters[3].value.as_blob().size() == 2);

    database.execute("UPDATE _app_view_filter_terms SET field_name = 'missing' WHERE position = 0;");
    CHECK_THROWS_AS(vulpes::appmeta::load_application_definition(database), vulpes::Error);
}
