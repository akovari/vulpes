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
    vulpes::ui::Grid grid{dataset, "Customers"};
    vulpes::terminal::ScreenBuffer buffer{40, 8};

    grid.render(buffer, {0, 0, 40, 8});

    CHECK(buffer.cell(0, 0).glyph == U'+');
    CHECK(buffer.cell(2, 0).glyph == U'C');
    CHECK(buffer.cell(1, 1).glyph == U'i');
    CHECK(buffer.cell(1, 3).glyph == U'1');
    CHECK(buffer.cell(1, 3).style.reverse);
}
