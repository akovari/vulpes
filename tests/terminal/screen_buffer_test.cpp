#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::terminal;

TEST_CASE("screen cells are addressable and Unicode-safe", "[terminal]") {
    ScreenBuffer buffer{80, 25};
    buffer.put(2, 3, U'✓', Style{.bold = true});
    CHECK(buffer.cell(2, 3).glyph == U'✓');
    CHECK(buffer.cell(2, 3).style.bold);
    CHECK_THROWS_AS(buffer.cell(80, 0), std::out_of_range);
}

TEST_CASE("Unicode helpers find the first UTF-8 code point", "[terminal]") {
    CHECK(first_code_point("Žlutý") == U'Ž');
    CHECK(first_code_point("plain") == U'p');
    CHECK(first_code_point("") == U'\0');
}
