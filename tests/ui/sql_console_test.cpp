#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/sql_console.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SQL console edits multiline Unicode scripts without executing database work", "[ui][sql]") {
    vulpes::ui::SqlConsole console{"SQL", "F8 Execute"};
    CHECK(console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'S'}) ==
          vulpes::ui::SqlConsoleResult::redraw);
    static_cast<void>(console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}));
    static_cast<void>(
        console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'ž'}));
    CHECK(console.script() == "S\nž");
    CHECK(console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) ==
          vulpes::ui::SqlConsoleResult::execute);

    console.set_status("1 row");
    vulpes::terminal::ScreenBuffer buffer{40, 8};
    console.render(buffer, {0, 0, 40, 8});
    CHECK(buffer.cell(0, 0).glyph == U'┌');
    CHECK(buffer.cell(3, 0).glyph == U'S');
    CHECK(buffer.cell(1, 1).glyph == U's');
    CHECK(buffer.cell(1, 1).style ==
          vulpes::ui::theme(vulpes::ui::ThemeName::midnight).style(vulpes::ui::ThemeRole::grid_footer));
    CHECK(buffer.cell(1, 2).style ==
          vulpes::ui::theme(vulpes::ui::ThemeName::midnight).style(vulpes::ui::ThemeRole::active_tab));
    CHECK(buffer.cell(1, 6).glyph == U'1');
}

TEST_CASE("SQL console provides cursor navigation and a scrolling editor viewport", "[ui][sql][editor]") {
    vulpes::ui::SqlConsole console{"SQL", "F8 Execute"};
    for (const auto character : std::u32string{U"abc"})
        static_cast<void>(console.handle(
            vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = character}));
    static_cast<void>(console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}));
    for (const auto character : std::u32string{U"def"})
        static_cast<void>(console.handle(
            vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = character}));
    static_cast<void>(console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::up}));
    static_cast<void>(console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::home}));
    static_cast<void>(
        console.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'X'}));
    CHECK(console.script() == "Xabc\ndef");

    vulpes::terminal::ScreenBuffer buffer{40, 8};
    console.render(buffer, {0, 0, 40, 8});
    CHECK(buffer.cell(1, 1).glyph == U's');
    CHECK(buffer.cell(6, 1).glyph == U'X');
    CHECK(buffer.cell(7, 1).style.reverse);
}
