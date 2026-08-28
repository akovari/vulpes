#include "vulpes/ui/multiline_editor.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

class MemoryClipboard final : public vulpes::core::Clipboard {
  public:
    auto read_text() -> std::optional<std::string> override { return text; }
    auto write_text(std::string_view value) -> bool override {
        text = value;
        return true;
    }

    std::optional<std::string> text;
};

} // namespace

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

TEST_CASE("multiline editor selects words and exchanges multiline clipboard text", "[ui][editor][clipboard]") {
    vulpes::ui::MultilineEditor editor{"select name\nfrom people"};
    MemoryClipboard clipboard;
    CHECK(editor.handle({.key = vulpes::terminal::Key::left, .ctrl = true, .shift = true}, &clipboard) ==
          vulpes::ui::MultilineEditResult::cursor_moved);
    CHECK(editor.selected_text() == "people");
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::insert_key, .ctrl = true}, &clipboard));
    REQUIRE(clipboard.text);
    CHECK(*clipboard.text == "people");
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::delete_key, .shift = true}, &clipboard));
    CHECK(editor.text() == "select name\nfrom ");
    clipboard.text = "orders\r\nwhere active";
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::insert_key, .shift = true}, &clipboard));
    CHECK(editor.text() == "select name\nfrom orders\nwhere active");
}

TEST_CASE("multiline editor provides bounded undo redo and invalidates redo on edits", "[ui][editor][undo]") {
    vulpes::ui::MultilineEditor editor;
    for (int index = 0; index < 120; ++index)
        static_cast<void>(editor.handle({.key = vulpes::terminal::Key::character, .character = U'x'}));
    CHECK(editor.can_undo());
    for (int index = 0; index < 100; ++index)
        CHECK(editor.handle({.key = vulpes::terminal::Key::character, .character = U'z', .ctrl = true}) ==
              vulpes::ui::MultilineEditResult::changed);
    CHECK(editor.text().size() == 20);
    CHECK_FALSE(editor.can_undo());
    CHECK(editor.can_redo());
    CHECK(editor.handle({.key = vulpes::terminal::Key::character, .character = U'y', .ctrl = true}) ==
          vulpes::ui::MultilineEditResult::changed);
    CHECK(editor.text().size() == 21);
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::character, .character = U'!'}));
    CHECK_FALSE(editor.can_redo());
}

TEST_CASE("multiline viewport reports visible selection columns", "[ui][editor][selection]") {
    vulpes::ui::MultilineEditor editor{"one\ntwo"};
    static_cast<void>(editor.handle({.key = vulpes::terminal::Key::home, .ctrl = true, .shift = true}));
    const auto view = editor.viewport(8, 2);
    REQUIRE(view.lines.size() == 2);
    REQUIRE(view.lines[0].selection_columns);
    REQUIRE(view.lines[1].selection_columns);
    CHECK(*view.lines[0].selection_columns == std::pair{0, 3});
    CHECK(*view.lines[1].selection_columns == std::pair{0, 3});
}
