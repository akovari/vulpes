#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/workspace.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <utility>

TEST_CASE("workspace text uses the selected Czech catalog for labels and mnemonic routing", "[ui][workspace][i18n]") {
    vulpes::core::Localizer messages{"cs-CZ"};
    messages.load_catalog_file(std::filesystem::path{VULPES_SOURCE_DIR} / "translations" / "cs.json");
    const auto text = vulpes::ui::make_workspace_text(messages);
    CHECK(text.menu_bar[0] == "Soubor");
    CHECK(text.database_menu[4] == "Procházet vybranou tabulku");
    CHECK(text.browse_document == "Procházet {table}");
    CHECK(text.command_title == "Příkaz");
    CHECK(text.menu_bar_mnemonics[0] == U'S');
    CHECK(text.file_menu_mnemonics[1] == U'J');

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

TEST_CASE("workspace text rejects colliding or absent explicit menu mnemonics", "[ui][workspace][i18n]") {
    auto catalog = vulpes::core::english_catalog();
    catalog["workspace.menu_bar.database.mnemonic"] = "F";
    vulpes::core::Localizer duplicate{"en"};
    duplicate.add_catalog("en", std::move(catalog));
    CHECK_THROWS_AS(vulpes::ui::make_workspace_text(duplicate), vulpes::Error);

    catalog = vulpes::core::english_catalog();
    catalog["workspace.menu.file.exit.mnemonic"] = "Q";
    vulpes::core::Localizer absent{"en"};
    absent.add_catalog("en", std::move(catalog));
    CHECK_THROWS_AS(vulpes::ui::make_workspace_text(absent), vulpes::Error);
}
