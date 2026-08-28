#include "vulpes/ui/window_manager.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("window manager owns tabs and gives modal Escape priority", "[ui][window]") {
    vulpes::ui::WindowManager windows{vulpes::ui::theme(vulpes::ui::ThemeName::midnight), "Workspace"};
    windows.open({.id = "browse:customer", .title = "Customers", .kind = vulpes::ui::DocumentKind::browse});
    windows.open({.id = "sql", .title = "SQL", .kind = vulpes::ui::DocumentKind::sql_console});
    REQUIRE(windows.documents().size() == 3);
    CHECK(windows.active().title == "SQL");
    windows.set_active_status("SQL ready");
    REQUIRE(windows.handle(vulpes::core::ActionId::workspace_next_document));
    CHECK(windows.active().title == "Workspace");
    windows.set_active_status("Workspace ready");
    REQUIRE(windows.handle(vulpes::core::ActionId::workspace_next_document));
    REQUIRE(windows.handle(vulpes::core::ActionId::workspace_next_document));
    CHECK(windows.active_status() == "SQL ready");

    windows.show_modal("Open database");
    windows.show_modal("Overwrite file");
    CHECK(windows.modal_depth() == 2);
    REQUIRE(windows.modal_title());
    CHECK(*windows.modal_title() == "Overwrite file");
    CHECK_FALSE(windows.handle(vulpes::core::ActionId::workspace_next_document));
    REQUIRE(windows.handle(vulpes::core::ActionId::application_back));
    REQUIRE(windows.modal_title());
    CHECK(*windows.modal_title() == "Open database");
    windows.dismiss_all_modals();
    CHECK_FALSE(windows.modal_title());

    windows.reset_documents();
    CHECK(windows.documents().size() == 1);
    CHECK(windows.active().title == "Workspace");
    CHECK(windows.active_status() == "Workspace ready");
}

TEST_CASE("window manager marks dirty documents without changing their identity", "[ui][window][dirty]") {
    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    vulpes::ui::WindowManager windows{theme, "Workspace"};
    windows.open({.id = "sql", .title = "SQL", .kind = vulpes::ui::DocumentKind::sql_console});
    windows.set_active_dirty(true);
    vulpes::terminal::ScreenBuffer buffer{30, 1};
    windows.render_tabs(buffer, {0, 0, 30, 1});

    CHECK(windows.active().id == "sql");
    CHECK(windows.active().dirty);
    CHECK(buffer.cell(18, 0).glyph == U'*');
}

TEST_CASE("window manager keeps the active Unicode tab visible in narrow layouts", "[ui][window]") {
    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    vulpes::ui::WindowManager windows{theme, "Workspace"};
    windows.open({.id = "one", .title = "Příliš dlouhá tabulka", .kind = vulpes::ui::DocumentKind::browse});
    windows.open({.id = "two", .title = "Orders", .kind = vulpes::ui::DocumentKind::browse});
    windows.open({.id = "sql", .title = "SQL", .kind = vulpes::ui::DocumentKind::sql_console});
    vulpes::terminal::ScreenBuffer buffer{24, 2};

    windows.render_tabs(buffer, {0, 0, 24, 1});

    CHECK(buffer.cell(18, 0).glyph == U'S');
    CHECK(buffer.cell(18, 0).style == theme.style(vulpes::ui::ThemeRole::active_tab));
}
