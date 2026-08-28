#include "vulpes/core/actions.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/frame_diff.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/workspace.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string_view>

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

TEST_CASE("workspace requests an explicit read-only database open", "[ui][workspace]") {
    auto workspace = english_workspace();
    CHECK(workspace.handle(
              vulpes::core::ActionId::database_open_read_only,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'r', .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    static_cast<void>(
        workspace.handle(vulpes::core::ActionId::none,
                         vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'a'}));
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::open_database_read_only);
    CHECK(workspace.requested_path() == "a");
}

TEST_CASE("workspace opens a selected recent database from its home screen", "[ui][workspace]") {
    auto workspace = english_workspace();
    workspace.set_recent_databases({"first.db", "second.db"});
    vulpes::terminal::ScreenBuffer buffer{80, 25};

    workspace.render(buffer, {0, 0, 80, 25});
    CHECK(buffer.cell(3, 8).glyph == U'R');
    CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::open_database);
    CHECK(workspace.requested_path() == "second.db");
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

TEST_CASE("workspace menus render classic chrome, shortcuts, disabled states, and item mnemonics", "[ui][workspace]") {
    auto workspace = english_workspace();
    vulpes::terminal::ScreenBuffer buffer{80, 25};

    CHECK(workspace.handle(vulpes::core::ActionId::application_menu,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f10}) ==
          vulpes::ui::WorkspaceResult::redraw);
    workspace.render(buffer, {0, 0, 80, 25});
    CHECK(buffer.cell(1, 1).glyph == U'┌');
    CHECK(buffer.cell(2, 2).glyph == U'►');
    CHECK(buffer.cell(4, 2).glyph == U'O');
    CHECK(buffer.cell(4, 2).style.underline);
    CHECK(buffer.cell(28, 2).glyph == U'C');
    CHECK(buffer.cell(18, 3).glyph == U'r');
    CHECK(buffer.cell(18, 3).style.underline);

    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(
              vulpes::core::ActionId::none,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'd', .alt = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    workspace.render(buffer, {0, 0, 80, 25});
    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    CHECK(buffer.cell(12, 7).style == theme.style(vulpes::ui::ThemeRole::disabled));

    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::application_menu,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f10}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'c'}) ==
          vulpes::ui::WorkspaceResult::redraw);
    static_cast<void>(
        workspace.handle(vulpes::core::ActionId::none,
                         vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'x'}));
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::create_database);
}

TEST_CASE("workspace path prompt remains visible at the minimum supported width", "[ui][workspace]") {
    auto workspace = english_workspace();
    vulpes::terminal::ScreenBuffer buffer{40, 10};
    CHECK(workspace.handle(vulpes::core::ActionId::database_open, {}) == vulpes::ui::WorkspaceResult::redraw);

    workspace.render(buffer, {0, 0, 40, 10});

    CHECK(buffer.cell(0, 2).glyph == U'┌');
    CHECK(buffer.cell(3, 2).glyph == U'O');
    CHECK(buffer.cell(0, 3).glyph == U'│');
}

TEST_CASE("workspace launches and dismisses the optional directory browser", "[ui][workspace]") {
    auto workspace = english_workspace();
    CHECK(workspace.handle(vulpes::core::ActionId::application_menu,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f10}) ==
          vulpes::ui::WorkspaceResult::redraw);
    for (int count = 0; count < 2; ++count) {
        CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                               vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
              vulpes::ui::WorkspaceResult::redraw);
    }
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
}

TEST_CASE("workspace command prompt submits semantic command text and preserves Escape cancellation",
          "[ui][workspace]") {
    auto workspace = english_workspace();
    CHECK(workspace.handle(
              vulpes::core::ActionId::application_command_palette,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'p', .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    for (const auto character : std::string_view{"browse customers"}) {
        static_cast<void>(workspace.handle(
            vulpes::core::ActionId::none,
            vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character,
                                       .character = static_cast<char32_t>(static_cast<unsigned char>(character))}));
    }
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::command);
    CHECK(workspace.requested_command() == "browse customers");

    CHECK(workspace.handle(
              vulpes::core::ActionId::application_command_palette,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'p', .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.requested_command().empty());
}

TEST_CASE("workspace asks before closing a document and clears stale tabs for a new database", "[ui][workspace]") {
    auto workspace = english_workspace();
    workspace.set_database("first.db", {{.name = "customers"}});
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::browse_table);
    CHECK(workspace.has_document("browse:customers"));

    CHECK(workspace.handle(
              vulpes::core::ActionId::workspace_close_document,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'w', .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.has_document("browse:customers"));
    CHECK(workspace.handle(vulpes::core::ActionId::application_back,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.has_document("browse:customers"));

    CHECK(workspace.handle(
              vulpes::core::ActionId::workspace_close_document,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'w', .ctrl = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::grid_next_column,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::right}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK_FALSE(workspace.has_document("browse:customers"));

    workspace.set_database("second.db", {{.name = "orders"}});
    CHECK_FALSE(workspace.has_document("browse:customers"));
    CHECK(workspace.active_document().kind == vulpes::ui::DocumentKind::workspace);
}

TEST_CASE("workspace Database menu opens browse and SQL commands", "[ui][workspace]") {
    auto workspace = english_workspace();
    workspace.set_database("workshop.db", {{.name = "customers"}});

    CHECK(workspace.handle(
              vulpes::core::ActionId::none,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'd', .alt = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    for (int count = 0; count < 4; ++count) {
        CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                               vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
              vulpes::ui::WorkspaceResult::redraw);
    }
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::browse_table);

    CHECK(workspace.handle(
              vulpes::core::ActionId::none,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'd', .alt = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
    for (int count = 0; count < 5; ++count) {
        CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                               vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
              vulpes::ui::WorkspaceResult::redraw);
    }
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::run_sql);
}

TEST_CASE("workspace application home routes metadata menu items as semantic commands", "[ui][workspace][appmeta]") {
    auto workspace = english_workspace();
    workspace.set_database("inventory.vulpes", {{.name = "products"}});
    workspace.set_application("Inventory", {{.name = "main",
                                             .label = "Main",
                                             .items = {{.label = "Products", .command = "products"},
                                                       {.label = "Low stock", .command = "low-stock"}}}});
    vulpes::terminal::ScreenBuffer buffer{80, 25};
    workspace.render(buffer, {0, 0, 80, 25});

    CHECK(buffer.cell(4, 7).glyph == U'M');
    CHECK(workspace.handle(vulpes::core::ActionId::dataset_next,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
          vulpes::ui::WorkspaceResult::redraw);
    CHECK(workspace.handle(vulpes::core::ActionId::none,
                           vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::WorkspaceResult::command);
    CHECK(workspace.requested_command() == "run \"low-stock\"");
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
