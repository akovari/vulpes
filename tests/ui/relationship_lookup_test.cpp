#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/relationship_lookup.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("relationship lookup searches selects and drills into related rows", "[ui][lookup]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT, code TEXT);"
                     "INSERT INTO customer VALUES(1, 'Acme', 'A'), (2, 'Beta', 'B');"
                     "CREATE TABLE job(id INTEGER PRIMARY KEY, customer_id INTEGER REFERENCES customer(id));");
    const auto schema = vulpes::db::inspect_schema(database);
    const auto job = std::ranges::find(schema, "job", &vulpes::db::TableSchema::name);
    REQUIRE(job != schema.end());
    vulpes::model::Dataset dataset{database, *job};
    vulpes::ui::RelationshipLookup lookup{dataset,
                                          "customer_id",
                                          {.display_field = "name", .search_fields = {"name", "code"}, .limit = 20},
                                          true,
                                          "Customer",
                                          "Search:",
                                          "Enter Select  F2 View  Esc Cancel"};

    REQUIRE(lookup.options().size() == 2);
    CHECK(lookup.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'B'}) ==
          vulpes::ui::RelationshipLookupResult::redraw);
    REQUIRE(lookup.options().size() == 1);
    CHECK(lookup.selected_option()->label == "Beta");
    CHECK(lookup.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f2}) ==
          vulpes::ui::RelationshipLookupResult::drill_down);
    CHECK(lookup.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::RelationshipLookupResult::selected);

    const auto record = dataset.related_record("customer_id", lookup.selected_option()->value);
    REQUIRE(record);
    vulpes::ui::RelatedRecordView view{*record, "Customer", "Esc Back"};
    vulpes::terminal::ScreenBuffer buffer{48, 10};
    view.render(buffer, {0, 0, 48, 10});
    CHECK(view.field_count() == 3);
    CHECK(buffer.cell(19, 2).glyph == U'B');
    CHECK(view.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::RelatedRecordResult::cancelled);
}
