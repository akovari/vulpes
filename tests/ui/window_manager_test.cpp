#include "vulpes/ui/window_manager.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("window manager owns tabs and gives modal Escape priority", "[ui][window]") {
    vulpes::ui::WindowManager windows{vulpes::ui::theme(vulpes::ui::ThemeName::midnight), "Workspace"};
    windows.open({.id = "browse:customer", .title = "Customers", .kind = vulpes::ui::DocumentKind::browse});
    windows.open({.id = "sql", .title = "SQL", .kind = vulpes::ui::DocumentKind::sql_console});
    REQUIRE(windows.documents().size() == 3);
    CHECK(windows.active().title == "SQL");
    REQUIRE(windows.handle(vulpes::core::ActionId::workspace_next_document));
    CHECK(windows.active().title == "Workspace");

    windows.show_modal("Open database");
    CHECK_FALSE(windows.handle(vulpes::core::ActionId::workspace_next_document));
    REQUIRE(windows.handle(vulpes::core::ActionId::application_back));
    CHECK_FALSE(windows.modal_title());
}
