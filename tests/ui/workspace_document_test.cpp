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

    CHECK(document.handle(vulpes::core::ActionId::record_new,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::insert_key}) ==
          vulpes::ui::DocumentResult::redraw);
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
    CHECK(document.handle(vulpes::core::ActionId::none, vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) ==
          vulpes::ui::DocumentResult::redraw);

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
