#include "vulpes/core/browse_controller.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("browse controller converts normalized keys into dataset movement", "[core][browse]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY); INSERT INTO sample VALUES (1), (2), (3)");
    vulpes::model::Dataset dataset{database, vulpes::db::inspect_schema(database).front(), 2};
    vulpes::core::BrowseController controller{dataset};

    CHECK(controller.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) == vulpes::core::BrowseResult::redraw);
    CHECK(dataset.current()->at("id").as_int() == 2);
    CHECK(controller.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) == vulpes::core::BrowseResult::redraw);
    CHECK(dataset.current()->at("id").as_int() == 3);
    CHECK(controller.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) == vulpes::core::BrowseResult::close);
}
