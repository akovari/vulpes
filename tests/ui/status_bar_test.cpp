#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/status_bar.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("status bar renders shortcut keys separately from labels", "[ui][status]") {
    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    const std::array hints{vulpes::ui::ShortcutHint{"F10", " Menu"}, vulpes::ui::ShortcutHint{"^O", " Open"}};
    vulpes::terminal::ScreenBuffer buffer{24, 2};

    vulpes::ui::StatusBar::render(buffer, {0, 1, 24, 1}, theme, {}, hints);

    CHECK(buffer.cell(0, 1).style == theme.style(vulpes::ui::ThemeRole::status_bar));
    CHECK(buffer.cell(1, 1).glyph == U'F');
    CHECK(buffer.cell(1, 1).style == theme.style(vulpes::ui::ThemeRole::status_bar_shortcut));
    CHECK(buffer.cell(4, 1).glyph == U' ');
    CHECK(buffer.cell(4, 1).style == theme.style(vulpes::ui::ThemeRole::status_bar));
}

TEST_CASE("status bar gives a status message priority over shortcut hints", "[ui][status]") {
    const auto& theme = vulpes::ui::theme(vulpes::ui::ThemeName::midnight);
    const std::array hints{vulpes::ui::ShortcutHint{"F10", " Menu"}};
    vulpes::terminal::ScreenBuffer buffer{20, 1};

    vulpes::ui::StatusBar::render(buffer, {0, 0, 20, 1}, theme, "Ready", hints);

    CHECK(buffer.cell(1, 0).glyph == U'R');
    CHECK(buffer.cell(1, 0).style == theme.style(vulpes::ui::ThemeRole::status_bar));
}
