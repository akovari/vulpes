#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/confirmation_dialog.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("confirmation dialog defaults to cancel and requires deliberate confirmation", "[ui][dialog]") {
    vulpes::ui::ConfirmationDialog dialog{"Delete", "Delete this record?", "Delete", "Cancel", "Enter Apply"};
    CHECK_FALSE(dialog.confirmed());
    CHECK(dialog.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::ConfirmationResult::cancelled);

    static_cast<void>(dialog.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::right}));
    CHECK(dialog.confirmed());
    static_cast<void>(dialog.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::tab, .shift = true}));
    CHECK_FALSE(dialog.confirmed());
    static_cast<void>(dialog.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::tab}));
    CHECK(dialog.confirmed());
    CHECK(dialog.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::ConfirmationResult::confirmed);

    vulpes::terminal::ScreenBuffer buffer{40, 8};
    dialog.render(buffer, {0, 1, 40, 6});
    CHECK(buffer.cell(3, 1).glyph == U'D');
    CHECK(buffer.cell(0, 2).glyph == U'│');
    CHECK(buffer.cell(1, 7).style ==
          vulpes::ui::theme(vulpes::ui::ThemeName::midnight).style(vulpes::ui::ThemeRole::shadow));
}
