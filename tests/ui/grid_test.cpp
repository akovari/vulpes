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

    CHECK(buffer.cell(0, 0).glyph == U'+');
    CHECK(buffer.cell(2, 0).glyph == U'C');
    CHECK(buffer.cell(1, 1).glyph == U'i');
    CHECK(buffer.cell(1, 3).glyph == U'1');
    CHECK(buffer.cell(1, 3).style.reverse);
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
    CHECK(buffer.cell(8, 1).style.underline);

    CHECK(grid.move_left());
    CHECK(grid.selected_column_index() == 1);
    REQUIRE(grid.selected_field());
    CHECK(grid.selected_field()->name == "first");
}
