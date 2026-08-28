#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/button.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("button measures and renders its focused state", "[ui][button]") {
    vulpes::ui::Button button{"Save"};
    vulpes::terminal::ScreenBuffer buffer{16, 3};

    CHECK(button.measure_width() == 8);
    button.render(buffer, {2, 1, 8, 1}, true);

    CHECK(buffer.cell(2, 1).glyph == U'[');
    CHECK(buffer.cell(2, 1).style.reverse);
    CHECK(buffer.cell(9, 1).glyph == U']');
}
