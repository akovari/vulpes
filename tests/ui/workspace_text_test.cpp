#include "vulpes/core/localization.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/workspace.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

TEST_CASE("workspace text uses the selected Czech catalog for labels and mnemonic routing", "[ui][workspace][i18n]") {
    vulpes::core::Localizer messages{"cs-CZ"};
    messages.load_catalog_file(std::filesystem::path{VULPES_SOURCE_DIR} / "translations" / "cs.json");
    const auto text = vulpes::ui::make_workspace_text(messages);
    CHECK(text.menu_bar[0] == "Soubor");
    CHECK(text.database_menu[4] == "Procházet vybranou tabulku");
    CHECK(text.browse_document == "Procházet {table}");
    CHECK(text.command_title == "Příkaz");

    CHECK(text.recent_databases == "Nedávné databáze:");

    vulpes::ui::Workspace workspace{text};
    vulpes::terminal::ScreenBuffer buffer{80, 25};
    workspace.render(buffer, {0, 0, 80, 25});
    CHECK(buffer.cell(2, 0).glyph == U'S');
    CHECK(workspace.handle(
              vulpes::core::ActionId::none,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U's', .alt = true}) ==
          vulpes::ui::WorkspaceResult::redraw);
}
