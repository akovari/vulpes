#include "vulpes/ui/multiline_editor.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("multiline editor inserts splits and joins UTF-8 lines at the cursor", "[ui][editor][sql]") {
    vulpes::ui::MultilineEditor editor{"AžB"};
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::left}));
    CHECK(editor.handle({.key = vulpes::terminal::Key::enter}) == vulpes::ui::MultilineEditResult::changed);
    CHECK(editor.text() == "Až\nB");
    CHECK(editor.handle({.key = vulpes::terminal::Key::backspace}) == vulpes::ui::MultilineEditResult::changed);
    CHECK(editor.text() == "AžB");
    CHECK(editor.handle({.key = vulpes::terminal::Key::backspace}) == vulpes::ui::MultilineEditResult::changed);
    CHECK(editor.text() == "AB");
    CHECK(editor.cursor_offset() == 1);
}

TEST_CASE("multiline editor preserves a desired display column across vertical movement", "[ui][editor][sql]") {
    vulpes::ui::MultilineEditor editor{"abcd\nx\n12界4"};
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::up}));
    CHECK(editor.cursor_offset() == 6);
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::up}));
    CHECK(editor.cursor_offset() == 4);
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::down}));
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::down}));
    CHECK(editor.cursor_offset() == std::string{"abcd\nx\n12界4"}.size());
}

TEST_CASE("multiline editor keeps the caret visible vertically and horizontally", "[ui][editor][sql]") {
    vulpes::ui::MultilineEditor editor{"first\nsecond\nthird\nabcdefgh"};
    const auto trailing = editor.viewport(4, 2);
    REQUIRE(trailing.lines.size() == 2);
    CHECK(trailing.first_line == 2);
    CHECK(trailing.cursor_row == 1);
    CHECK(trailing.cursor_column == 3);
    CHECK(trailing.lines[1].text == "fgh");
    CHECK(trailing.lines[1].clipped_left);
    CHECK(trailing.clipped_above);
    CHECK_FALSE(trailing.clipped_below);

    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::home}));
    const auto leading = editor.viewport(4, 2);
    CHECK(leading.cursor_column == 0);
    CHECK(leading.lines[1].text == "abcd");
    CHECK(leading.lines[1].clipped_right);

    vulpes::ui::MultilineEditor wide{"界a"};
    static_cast<void>(wide.handle({.key = vulpes::terminal::Key::home}));
    static_cast<void>(wide.handle({.key = vulpes::terminal::Key::right}));
    const auto aligned = wide.viewport(2, 1);
    CHECK(aligned.cursor_column == 1);
    CHECK(aligned.lines[0].text == " a");
}

TEST_CASE("multiline editor supports page movement and tab stops", "[ui][editor][sql]") {
    vulpes::ui::MultilineEditor editor{"0\n1\n2\n3\n4"};
    static_cast<void>(editor.viewport(10, 3));
    CHECK(editor.handle({.key = vulpes::terminal::Key::page_up}) == vulpes::ui::MultilineEditResult::cursor_moved);
    CHECK(editor.cursor_offset() == 5);
    CHECK(editor.handle({.key = vulpes::terminal::Key::home}) == vulpes::ui::MultilineEditResult::cursor_moved);
    CHECK(editor.handle({.key = vulpes::terminal::Key::tab}) == vulpes::ui::MultilineEditResult::changed);
    CHECK(editor.text() == "0\n1\n    2\n3\n4");
}
