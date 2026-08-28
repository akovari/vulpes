#include "vulpes/core/localization.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/terminal_diagnostics.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("terminal diagnostics renders normalized key and resize events", "[ui][terminal]") {
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::TerminalDiagnostics diagnostics{messages};
    vulpes::terminal::ScreenBuffer buffer{80, 20};

    CHECK(diagnostics.handle(vulpes::core::ActionId::none,
                             vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::left, .ctrl = true}) ==
          vulpes::ui::DocumentResult::redraw);
    CHECK(diagnostics.handle(vulpes::core::ActionId::none, vulpes::terminal::ResizeEvent{.width = 100, .height = 30}) ==
          vulpes::ui::DocumentResult::redraw);
    diagnostics.render(buffer, {0, 0, 80, 20});

    CHECK(buffer.cell(3, 0).glyph == U'T');
    CHECK(buffer.cell(1, 1).glyph == U'K');
    CHECK(buffer.cell(1, 2).glyph == U'R');
    CHECK(diagnostics.handle(vulpes::core::ActionId::application_back,
                             vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::DocumentResult::close);
    CHECK(diagnostics.handle(
              vulpes::core::ActionId::application_quit,
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'c', .ctrl = true}) ==
          vulpes::ui::DocumentResult::close);
}
