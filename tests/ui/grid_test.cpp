#include "vulpes/appmeta/metadata.hpp"
#include "vulpes/core/formatting.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/ui/grid.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("grid renders dataset fields and selected row to logical cells", "[ui][grid]") {
    vulpes::db::Database database{":memory:"};
    database.execute(
        "CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT); INSERT INTO customer VALUES (1, 'Acme')");
    const auto schema = vulpes::db::inspect_schema(database).front();
    vulpes::model::Dataset dataset{database, schema};
    vulpes::ui::Grid grid{dataset, "Customers", "F2 Edit"};
    vulpes::terminal::ScreenBuffer buffer{40, 8};

    grid.render(buffer, {0, 0, 40, 8});

    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    CHECK(buffer.cell(0, 0).glyph == U'┌');
    CHECK(buffer.cell(3, 0).glyph == U'C');
    CHECK(buffer.cell(1, 1).glyph == U'i');
    CHECK(buffer.cell(1, 3).glyph == U'1');
    CHECK(buffer.cell(1, 3).style == theme.style(vulpes::ui::ThemeRole::grid_selected_cell));
    CHECK(buffer.cell(1, 6).glyph == U'F');
    REQUIRE(grid.selected_field());
    CHECK(grid.selected_field()->name == "id");
}

TEST_CASE("grid selects and scrolls columns independently of dataset navigation", "[ui][grid]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY, first TEXT, second TEXT, third TEXT, fourth TEXT);"
                     "INSERT INTO sample VALUES (1, 'a', 'b', 'c', 'd')");
    const auto schema = vulpes::db::inspect_schema(database).front();
    vulpes::model::Dataset dataset{database, schema};
    vulpes::ui::Grid grid{dataset, "Sample", "F2 Edit"};

    CHECK_FALSE(grid.move_left());
    REQUIRE(grid.move_right());
    REQUIRE(grid.move_right());
    CHECK(grid.selected_column_index() == 2);
    CHECK(grid.first_visible_column_index() == 0);

    vulpes::terminal::ScreenBuffer buffer{14, 8};
    grid.render(buffer, {0, 0, 14, 8});
    CHECK(grid.first_visible_column_index() == 1);
    CHECK(buffer.cell(0, 1).glyph == U'◀');
    CHECK(buffer.cell(13, 1).glyph == U'▶');
    CHECK(buffer.cell(8, 1).style.underline);

    CHECK(grid.move_left());
    CHECK(grid.selected_column_index() == 1);
    REQUIRE(grid.selected_field());
    CHECK(grid.selected_field()->name == "first");
}

TEST_CASE("grid follows selected rows and renders position and scrollbar affordances", "[ui][grid][scroll]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY, name TEXT);"
                     "INSERT INTO sample(name) VALUES ('a'),('b'),('c'),('d'),('e'),('f'),('g'),('h'),('i'),('j')");
    vulpes::model::Dataset dataset{database, vulpes::db::inspect_schema(database).front()};
    vulpes::ui::Grid grid{dataset, "Sample", "F2 Edit"};
    for (int index = 0; index < 5; ++index)
        REQUIRE(dataset.next());

    vulpes::terminal::ScreenBuffer buffer{40, 8};
    grid.render(buffer, {0, 0, 40, 8});

    CHECK(buffer.cell(1, 3).glyph == U'4');
    CHECK(buffer.cell(1, 5).glyph == U'6');
    CHECK(buffer.cell(39, 4).glyph == U'█');
    CHECK(buffer.cell(22, 6).glyph == U'R');
}

TEST_CASE("grid renders an empty state and remembers user column widths", "[ui][grid][empty][resize]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY, name TEXT)");
    vulpes::model::Dataset dataset{database, vulpes::db::inspect_schema(database).front()};
    vulpes::ui::Grid grid{dataset, "Sample", "F2 Edit"};
    vulpes::terminal::ScreenBuffer buffer{40, 8};

    grid.render(buffer, {0, 0, 40, 8});
    CHECK(buffer.cell(15, 4).glyph == U'N');
    const auto before = grid.selected_column_width();
    REQUIRE(before);
    REQUIRE(grid.resize_selected_column(-2));
    grid.render(buffer, {0, 0, 40, 8});
    REQUIRE(grid.selected_column_width());
    CHECK(*grid.selected_column_width() == *before - 2);
}

TEST_CASE("grid reuses its renderer for owned SQL results", "[ui][grid][sql]") {
    vulpes::db::Database database{":memory:"};
    const auto result = database.run_sql("SELECT 1 AS id, 'Acme' AS name UNION ALL SELECT 2, 'Delta'");
    const auto rows = vulpes::ui::GridRows::from_sql_result(result);
    vulpes::ui::Grid grid{rows, "Results", "Up/Down Rows"};
    vulpes::terminal::ScreenBuffer buffer{32, 8};

    grid.render(buffer, {0, 0, 32, 8});

    CHECK(buffer.cell(1, 3).glyph == U'1');
    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    CHECK(buffer.cell(1, 3).style == theme.style(vulpes::ui::ThemeRole::grid_selected_cell));
    REQUIRE(grid.move_next_row());
    grid.render(buffer, {0, 0, 32, 8});
    CHECK(buffer.cell(1, 4).style == theme.style(vulpes::ui::ThemeRole::grid_selected_cell));
}

TEST_CASE("grid applies locale-aware numeric display without changing stored values", "[ui][grid][i18n]") {
    vulpes::db::Database database{":memory:"};
    const auto rows = vulpes::ui::GridRows::from_sql_result(database.run_sql("SELECT 12345.5 AS amount"));
    const vulpes::core::LocaleFormatter formatter{"cs-CZ"};
    vulpes::ui::Grid grid{rows, "Values", "", vulpes::ui::theme(vulpes::ui::ThemeName::midnight), {}, formatter};
    vulpes::terminal::ScreenBuffer buffer{40, 8};

    grid.render(buffer, {0, 0, 40, 8});

    CHECK(buffer.cell(3, 3).glyph == U'\u00a0');
    CHECK(buffer.cell(7, 3).glyph == U',');
    CHECK(rows.rows.front().at("amount").as_double() == 12'345.5);
}

TEST_CASE("grid applies metadata currency and temporal presentation", "[ui][grid][i18n][appmeta]") {
    vulpes::db::Database database{":memory:"};
    const auto rows = vulpes::ui::GridRows::from_sql_result(
        database.run_sql("SELECT '2024-01-02' AS issued_on, 1234.5 AS total, 'internal' AS secret"));
    const vulpes::appmeta::TableMetadata metadata{
        .name = "invoice",
        .fields = {{.name = "issued_on", .label = "Issued", .order = 1, .format = vulpes::appmeta::FieldFormat::date},
                   {.name = "total",
                    .label = "Amount",
                    .order = 0,
                    .format = vulpes::appmeta::FieldFormat::currency,
                    .currency_code = "EUR"},
                   {.name = "secret", .visible = false}},
    };
    vulpes::ui::Grid grid{rows,    "Invoices",
                          "",      vulpes::ui::theme(vulpes::ui::ThemeName::midnight),
                          {},      vulpes::core::LocaleFormatter{"cs-CZ"},
                          metadata};
    vulpes::terminal::ScreenBuffer buffer{50, 8};

    grid.render(buffer, {0, 0, 50, 8});

    bool has_date_separator{false};
    bool has_currency_symbol{false};
    for (int column = 0; column < buffer.width(); ++column) {
        has_date_separator = has_date_separator || buffer.cell(column, 3).glyph == U'.';
        has_currency_symbol = has_currency_symbol || buffer.cell(column, 3).glyph == U'€';
    }
    CHECK(has_date_separator);
    CHECK(has_currency_symbol);
    REQUIRE(grid.selected_field());
    CHECK(grid.selected_field()->name == "total");
    CHECK(buffer.cell(1, 1).glyph == U'A');
    CHECK(grid.move_right());
    CHECK_FALSE(grid.move_right());
    CHECK(rows.rows.front().at("issued_on").as_string() == "2024-01-02");
    CHECK(rows.rows.front().at("total").as_double() == 1234.5);
}

TEST_CASE("grid applies an injected palette to document cells", "[ui][grid][theme]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY, name TEXT); INSERT INTO sample VALUES(1, 'Acme')");
    vulpes::model::Dataset dataset{database, vulpes::db::inspect_schema(database).front()};
    const auto& high_contrast = vulpes::ui::theme(vulpes::ui::ThemeName::high_contrast);
    vulpes::ui::Grid grid{dataset, "Sample", "Footer", high_contrast};
    vulpes::terminal::ScreenBuffer buffer{30, 8};

    grid.render(buffer, {0, 0, 30, 8});

    CHECK(buffer.cell(16, 1).style == high_contrast.style(vulpes::ui::ThemeRole::grid_header));
    CHECK(buffer.cell(1, 3).style == high_contrast.style(vulpes::ui::ThemeRole::grid_selected_cell));
    CHECK(buffer.cell(1, 6).style == high_contrast.style(vulpes::ui::ThemeRole::grid_footer));
}
