#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/text_prompt.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("text prompt edits Unicode text and exposes its terminal-independent state", "[ui][prompt]") {
    vulpes::ui::TextPrompt prompt{"Search", "Enter Apply"};
    CHECK(prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'A'}) ==
          vulpes::ui::PromptResult::redraw);
    static_cast<void>(
        prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'ž'}));
    CHECK(prompt.value() == "Až");
    static_cast<void>(prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::backspace}));
    CHECK(prompt.value() == "A");
    CHECK(prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::PromptResult::submitted);

    vulpes::terminal::ScreenBuffer buffer{40, 8};
    prompt.render(buffer, {0, 1, 40, 5});
    CHECK(buffer.cell(3, 1).glyph == U'S');
    CHECK(buffer.cell(3, 2).glyph == U'A');
    CHECK(buffer.cell(3, 2).style ==
          vulpes::ui::theme(vulpes::ui::ThemeName::midnight).style(vulpes::ui::ThemeRole::input_focus));
}

TEST_CASE("text prompt edits at the logical cursor instead of appending", "[ui][prompt][editor]") {
    vulpes::ui::TextPrompt prompt{"Path", "Enter Apply", "ac"};
    CHECK(prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::left}) ==
          vulpes::ui::PromptResult::redraw);
    CHECK(prompt.cursor_offset() == 1);
    CHECK(prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'b'}) ==
          vulpes::ui::PromptResult::redraw);
    CHECK(prompt.value() == "abc");
    CHECK(prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::home}) ==
          vulpes::ui::PromptResult::redraw);
    CHECK(prompt.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::delete_key}) ==
          vulpes::ui::PromptResult::redraw);
    CHECK(prompt.value() == "bc");
}
