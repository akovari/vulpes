#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("window frame is opaque and exposes its logical content bounds", "[ui][window]") {
    vulpes::terminal::ScreenBuffer buffer{20, 8};
    buffer.put(5, 3, U'X');

    vulpes::ui::WindowFrame::render(buffer, {2, 1, 12, 6}, "Title");

    CHECK(vulpes::ui::WindowFrame::fits(buffer, {2, 1, 12, 6}, 12, 6));
    CHECK(vulpes::ui::WindowFrame::content_bounds({2, 1, 12, 6}) == vulpes::ui::Rect{3, 2, 10, 4});
    CHECK(buffer.cell(2, 1).glyph == U'+');
    CHECK(buffer.cell(4, 1).glyph == U'T');
    CHECK(buffer.cell(5, 3).glyph == U' ');
}
