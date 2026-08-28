#include "vulpes/core/actions.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/frame_diff.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/workspace.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

auto english_workspace() -> vulpes::ui::Workspace {
    vulpes::core::Localizer messages{"en"};
    return vulpes::ui::Workspace{vulpes::ui::make_workspace_text(messages)};
}

} // namespace

TEST_CASE("workspace opens path modal and selects database tables", "[ui][workspace]") {
    auto workspace = english_workspace();
    CHECK(workspace.handle(vulpes::core::ActionId::database_open, {}) == vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
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
    CHECK(workspace.handle(vulpes::core::ActionId::workspace_next_document,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::tab, .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::workspace_next_document,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::tab, .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(
              vulpes::core::ActionId::workspace_close_document,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'w', .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
}

TEST_CASE("workspace menu navigation supports arrows, mnemonics, and Escape", "[ui][workspace]") {
    auto workspace = english_workspace();
    CHECK(workspace.handle(vulpes::core::ActionId::application_menu,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f10}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::grid_previous_column,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::left}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::grid_next_column,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::right}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(
              vulpes::core::ActionId::application_menu,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'f', .alt = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
}

TEST_CASE("workspace Database menu opens browse and SQL commands", "[ui][workspace]") {
    auto workspace = english_workspace();
    workspace.set_database("workshop.db", {{.name = "customers"}});

    CHECK(workspace.handle(
              vulpes::core::ActionId::none,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'd', .alt = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::browse_table);

    CHECK(workspace.handle(
              vulpes::core::ActionId::none,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'd', .alt = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    for (int count = 0; count < 3; ++count) {
        CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                               vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
              vulpes::ui::WorkspaceResult::redraw);
    }
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::run_sql);
}

TEST_CASE("workspace clears a dismissed modal from the next frame", "[ui][workspace]") {
    auto workspace = english_workspace();
    vulpes::terminal::ScreenBuffer modal_frame{80, 25};
    vulpes::terminal::ScreenBuffer next_frame{80, 25};

    CHECK(workspace.handle(vulpes::core::ActionId::database_open, {}) == vulpes::ui::WorkspaceResult::redraw);
    workspace.render(modal_frame, {.x = 0, .y = 0, .width = 80, .height = 25});
    CHECK(modal_frame.cell(10, 10).glyph != U' ');

    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
    workspace.render(next_frame, {.x = 0, .y = 0, .width = 80, .height = 25});

    CHECK(next_frame.cell(10, 10).glyph == U' ');
    CHECK_FALSE(vulpes::terminal::diff_frames(modal_frame, next_frame).empty());
}
