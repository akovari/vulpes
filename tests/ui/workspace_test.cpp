#include "vulpes/core/actions.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/ui/workspace.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("workspace opens path modal and selects database tables", "[ui][workspace]") {
    vulpes::ui::Workspace workspace{"Vulpes", "Open", "Create", "Enter path"};
    CHECK(workspace.handle(vulpes::core::ActionId::database_open, {}) == vulpes::ui::WorkspaceResult::redraw);
    static_cast<void>(
        workspace.handle(vulpes::core::ActionId::none,
                         vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'a'}));
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::open_database);
    CHECK(workspace.requested_path() == "a");

    workspace.set_database("a.db", {{.name = "customers"}, {.name = "jobs"}});
    REQUIRE(workspace.selected_table());
    CHECK(workspace.selected_table()->name == "customers");
    CHECK(workspace.handle(vulpes::core::ActionId::dataset_next, {}) == vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.selected_table()->name == "jobs");
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::browse_table);
}
