#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/test_terminal.hpp"
#include "vulpes/ui/browse_document.hpp"
#include "vulpes/ui/document_session.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("document session hosts a browse document through the terminal abstraction", "[ui][document]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customers (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    database.execute("INSERT INTO customers(name) VALUES ('Acme')");
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::BrowseDocument document{database, vulpes::db::inspect_schema(database).front(), messages};
    vulpes::terminal::TestTerminal terminal{{80, 22}};
    terminal.enqueue(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape});

    vulpes::ui::DocumentSession session{terminal, document, {20, 6}, "Terminal is too small."};
    session.run();

    REQUIRE(terminal.frames().size() == 1);
    CHECK(terminal.frames().front().cell(2, 0).glyph == U'c');
}

TEST_CASE("document session shows a resize warning and still permits a clean exit", "[ui][document]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customers (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::BrowseDocument document{database, vulpes::db::inspect_schema(database).front(), messages};
    vulpes::terminal::TestTerminal terminal{{18, 5}};
    terminal.enqueue(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape});

    vulpes::ui::DocumentSession session{terminal, document, {20, 6}, "Terminal is too small."};
    session.run();

    REQUIRE(terminal.frames().size() == 1);
    CHECK(terminal.frames().front().cell(0, 2).glyph == U'T');
}
