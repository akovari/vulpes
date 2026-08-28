#include "vulpes/appmeta/definition.hpp"
#include "vulpes/core/actions.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/report_document.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("report document executes a bounded read-only query and navigates shared Grid", "[ui][report]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT);"
                     "INSERT INTO customer(name) VALUES('Acme'),('Beta')");
    const vulpes::core::Localizer messages{"en"};
    vulpes::ui::ReportDocument document{database,
                                        {.name = "customers",
                                         .label = "Customers",
                                         .sql = "SELECT id, name FROM customer ORDER BY id",
                                         .row_limit = 100},
                                        messages};
    vulpes::terminal::ScreenBuffer buffer{48, 10};

    document.render(buffer, {0, 0, 48, 10});
    CHECK(buffer.cell(1, 3).glyph == U'1');
    CHECK(document.handle(vulpes::core::ActionId::dataset_next,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
          vulpes::ui::DocumentResult::redraw);
    CHECK(document.handle(vulpes::core::ActionId::application_back,
                          vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::DocumentResult::close);
}

TEST_CASE("report document refuses metadata SQL that can modify the database", "[ui][report]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY)");
    const vulpes::core::Localizer messages{"en"};

    CHECK_THROWS_AS(
        vulpes::ui::ReportDocument(database, {.name = "bad", .label = "Bad", .sql = "DELETE FROM customer"}, messages),
        vulpes::Error);
}

TEST_CASE("report document identifies a row-limited result", "[ui][report][limit]") {
    vulpes::db::Database database{":memory:"};
    const vulpes::core::Localizer messages{"en"};
    vulpes::ui::ReportDocument document{
        database,
        {.name = "limited", .label = "Limited", .sql = "SELECT 1 AS value UNION ALL SELECT 2", .row_limit = 1},
        messages};
    vulpes::terminal::ScreenBuffer buffer{64, 10};

    document.render(buffer, {0, 0, 64, 10});

    bool rendered_truncation_notice{false};
    for (int column = 0; column < buffer.width(); ++column)
        rendered_truncation_notice = rendered_truncation_notice || buffer.cell(column, 8).glyph == U't';
    CHECK(rendered_truncation_notice);
}
