#include "vulpes/terminal/screen_buffer.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::terminal;

TEST_CASE("screen cells are addressable and Unicode-safe", "[terminal]") {
    ScreenBuffer buffer{80, 25};
    buffer.put(2, 3, U'✓', Style{.bold = true});
    CHECK(buffer.cell(2, 3).glyph == U'✓');
    CHECK(buffer.cell(2, 3).style.bold);
    CHECK_THROWS_AS(buffer.cell(80, 0), std::out_of_range);
}

