#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/line_editor.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("line editor inserts and removes UTF-8 only at code-point boundaries", "[ui][editor]") {
    vulpes::ui::LineEditor editor{"AžB"};
    CHECK(editor.cursor_offset() == std::string{"AžB"}.size());

    CHECK(editor.handle({.key = vulpes::terminal::Key::left}) == vulpes::ui::LineEditResult::cursor_moved);
    CHECK(editor.handle({.key = vulpes::terminal::Key::backspace}) == vulpes::ui::LineEditResult::changed);
    CHECK(editor.text() == "AB");
    CHECK(editor.cursor_offset() == 1);
    CHECK(editor.handle({.key = vulpes::terminal::Key::character, .character = U'č'}) ==
          vulpes::ui::LineEditResult::changed);
    CHECK(editor.text() == "AčB");
    CHECK(editor.handle({.key = vulpes::terminal::Key::delete_key}) == vulpes::ui::LineEditResult::changed);
    CHECK(editor.text() == "Ač");
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::home}));
    CHECK(editor.handle({.key = vulpes::terminal::Key::delete_key}) == vulpes::ui::LineEditResult::changed);
    CHECK(editor.text() == "č");
}

TEST_CASE("line editor supports Home End and horizontal cursor following", "[ui][editor]") {
    vulpes::ui::LineEditor editor{"abcdef"};
    const auto trailing = editor.viewport(4);
    CHECK(trailing.text == "def");
    CHECK(trailing.cursor_column == 3);
    CHECK(trailing.clipped_left);
    CHECK_FALSE(trailing.clipped_right);

    CHECK(editor.handle({.key = vulpes::terminal::Key::home}) == vulpes::ui::LineEditResult::cursor_moved);
    const auto leading = editor.viewport(4);
    CHECK(leading.text == "abcd");
    CHECK(leading.cursor_column == 0);
    CHECK_FALSE(leading.clipped_left);
    CHECK(leading.clipped_right);
    CHECK(editor.handle({.key = vulpes::terminal::Key::end}) == vulpes::ui::LineEditResult::cursor_moved);

    editor.set_text("A界B");
    const auto wide = editor.viewport(4);
    CHECK(wide.text == "界B");
    CHECK(wide.cursor_column == 3);
}

TEST_CASE("line editor renders a logical caret and a stable unfocused viewport", "[ui][editor]") {
    vulpes::ui::LineEditor editor{"abcdef"};
    vulpes::terminal::ScreenBuffer buffer{8, 2};
    const vulpes::terminal::Style style{.foreground = {1, 2, 3}, .background = {4, 5, 6}};

    editor.render(buffer, {0, 0, 4, 1}, style, true);
    CHECK(buffer.cell(0, 0).glyph == U'd');
    CHECK(buffer.cell(3, 0).style.reverse);
    editor.render(buffer, {0, 1, 4, 1}, style, false);
    CHECK(buffer.cell(0, 1).glyph == U'a');
    CHECK_FALSE(buffer.cell(0, 1).style.reverse);
}
