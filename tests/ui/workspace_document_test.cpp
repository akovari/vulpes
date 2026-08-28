#include "vulpes/appmeta/metadata.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/browse_document.hpp"
#include "vulpes/ui/sql_document.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("workspace browse document owns transient forms and closes semantically", "[ui][workspace]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customers (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    database.execute("INSERT INTO customers(name) VALUES ('Acme')");
    const auto table = vulpes::db::inspect_schema(database).front();
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::BrowseDocument document{database, table, messages};
    vulpes::terminal::ScreenBuffer buffer{80, 22};

    document.render(buffer, {0, 0, 80, 22});
    CHECK(buffer.cell(3, 0).glyph == U'c');
    CHECK_FALSE(document.is_dirty());

    CHECK(document.handle(vulpes::core::ActionId::record_new,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::insert_key}) ==
          vulpes::ui::DocumentResult::redraw);
    CHECK_FALSE(document.is_dirty());
    CHECK(document.handle(vulpes::core::ActionId::none,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'X'}) ==
          vulpes::ui::DocumentResult::redraw);
    CHECK(document.is_dirty());
    CHECK(document.handle(vulpes::core::ActionId::application_back,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::DocumentResult::redraw);
    CHECK(document.handle(vulpes::core::ActionId::application_back,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::DocumentResult::close);
}

TEST_CASE("workspace SQL document closes without affecting its database", "[ui][workspace]") {
    vulpes::db::Database database{":memory:"};
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::SqlDocument document{database, messages};

    CHECK_FALSE(document.is_dirty());

    CHECK(document.handle(vulpes::core::ActionId::application_back,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::DocumentResult::close);
}

TEST_CASE("workspace SQL document switches keyboard focus between editor and results", "[ui][workspace][sql]") {
    vulpes::db::Database database{":memory:"};
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::SqlDocument document{database, messages};
    for (const auto character : std::u32string{U"SELECT 1 AS id"})
        static_cast<void>(document.handle(
            vulpes::core::ActionId::none,
            vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = character}));
    CHECK(document.is_dirty());
    CHECK(document.handle(vulpes::core::ActionId::none, vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) ==
          vulpes::ui::DocumentResult::redraw);
    CHECK_FALSE(document.is_dirty());

    vulpes::terminal::ScreenBuffer buffer{80, 22};
    document.render(buffer, {0, 0, 80, 22});
    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    CHECK(buffer.cell(1, 10).style == theme.style(vulpes::ui::ThemeRole::grid_selected_row));

    CHECK(document.handle(vulpes::core::ActionId::document_switch_pane,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f7}) ==
          vulpes::ui::DocumentResult::redraw);
    document.render(buffer, {0, 0, 80, 22});
    CHECK(buffer.cell(1, 10).style == theme.style(vulpes::ui::ThemeRole::grid_selected_cell));
    CHECK(document.handle(vulpes::core::ActionId::application_back,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::DocumentResult::redraw);
}

TEST_CASE("workspace browse document hosts searchable relationship selection", "[ui][workspace][lookup]") {
    vulpes::db::Database database{":memory:"};
    database.execute(
        "CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
        "INSERT INTO customer VALUES (1, 'Acme'), (2, 'Beta');"
        "CREATE TABLE job(id INTEGER PRIMARY KEY, customer_id INTEGER REFERENCES customer(id), description TEXT);");
    const auto schemas = vulpes::db::inspect_schema(database);
    const auto job = std::ranges::find(schemas, "job", &vulpes::db::TableSchema::name);
    REQUIRE(job != schemas.end());
    vulpes::appmeta::ApplicationMetadata metadata{{{
        .name = "job",
        .fields = {{.name = "customer_id",
                    .label = "Customer",
                    .lookup = vulpes::appmeta::LookupMetadata{.display_field = "name", .search_fields = {"name"}}}},
    }}};
    metadata.validate(schemas);
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::BrowseDocument document{
        database, *job, messages, vulpes::ui::theme(vulpes::ui::ThemeName::midnight), nullptr, &metadata};

    static_cast<void>(document.handle(vulpes::core::ActionId::record_new,
                                      vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::insert_key}));
    static_cast<void>(
        document.handle(vulpes::core::ActionId::none, vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    static_cast<void>(
        document.handle(vulpes::core::ActionId::none, vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}));
    static_cast<void>(
        document.handle(vulpes::core::ActionId::none,
                        vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'B'}));
    static_cast<void>(document.handle(vulpes::core::ActionId::record_edit,
                                      vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f2}));
    static_cast<void>(document.handle(vulpes::core::ActionId::application_back,
                                      vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}));
    static_cast<void>(
        document.handle(vulpes::core::ActionId::none, vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}));
    static_cast<void>(
        document.handle(vulpes::core::ActionId::none, vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    static_cast<void>(
        document.handle(vulpes::core::ActionId::none,
                        vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'J'}));
    CHECK(document.handle(vulpes::core::ActionId::none, vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) ==
          vulpes::ui::DocumentResult::redraw);

    auto query = database.prepare("SELECT customer_id, description FROM job");
    REQUIRE(query.step());
    CHECK(query.column(0).as_int() == 2);
    CHECK(query.column(1).as_string() == "J");
}
