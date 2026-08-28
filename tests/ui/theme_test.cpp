#include "vulpes/core/error.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/theme.hpp"
#include "vulpes/ui/workspace.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("theme parser accepts named workspace palettes", "[ui][theme]") {
    CHECK(vulpes::ui::parse_theme("midnight") == vulpes::ui::ThemeName::midnight);
    CHECK(vulpes::ui::parse_theme("HIGH-CONTRAST") == vulpes::ui::ThemeName::high_contrast);
    CHECK_THROWS_AS(vulpes::ui::parse_theme("neon"), vulpes::Error);
}

TEST_CASE("high contrast palette keeps all workspace roles distinguishable", "[ui][theme]") {
    const auto& high_contrast = vulpes::ui::theme(vulpes::ui::ThemeName::high_contrast);
    for (const auto role :
         {vulpes::ui::ThemeRole::text, vulpes::ui::ThemeRole::title, vulpes::ui::ThemeRole::menu,
          vulpes::ui::ThemeRole::menu_mnemonic, vulpes::ui::ThemeRole::selection, vulpes::ui::ThemeRole::popup,
          vulpes::ui::ThemeRole::popup_selection, vulpes::ui::ThemeRole::active_menu,
          vulpes::ui::ThemeRole::active_menu_mnemonic, vulpes::ui::ThemeRole::tab, vulpes::ui::ThemeRole::active_tab}) {
        const auto& style = high_contrast.style(role);
        CHECK(style.foreground != style.background);
    }
    CHECK(high_contrast.style(vulpes::ui::ThemeRole::menu_mnemonic).underline);
    CHECK(high_contrast.style(vulpes::ui::ThemeRole::active_menu_mnemonic).underline);
}

TEST_CASE("workspace renders its chrome through injected theme roles", "[ui][theme]") {
    const auto& high_contrast = vulpes::ui::theme(vulpes::ui::ThemeName::high_contrast);
    vulpes::ui::Workspace workspace{"Vulpes", "Open", "Create", "Enter path", high_contrast};
    vulpes::terminal::ScreenBuffer buffer{80, 25};

    workspace.render(buffer, {0, 0, 80, 25});

    CHECK(buffer.cell(0, 0).style == high_contrast.style(vulpes::ui::ThemeRole::menu));
    CHECK(buffer.cell(1, 0).style == high_contrast.style(vulpes::ui::ThemeRole::menu_mnemonic));
    CHECK(buffer.cell(1, 24).style == high_contrast.style(vulpes::ui::ThemeRole::menu_mnemonic));
}
