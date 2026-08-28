#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/terminal_warning.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("terminal warning renders a centered-row host message", "[ui][terminal]") {
    vulpes::terminal::ScreenBuffer buffer{30, 7};

    vulpes::ui::render_terminal_warning(buffer, {30, 7}, "Resize terminal");

    CHECK(buffer.cell(0, 3).glyph == U'R');
    CHECK(buffer.cell(0, 3).style.bold);
}
