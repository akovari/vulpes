#include "vulpes/core/error.hpp"
#include "vulpes/terminal/frame_diff.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::terminal;

TEST_CASE("frame diff emits only changed cells", "[terminal][render]") {
    ScreenBuffer previous{5, 2};
    ScreenBuffer current{5, 2};
    current.put(1, 0, U'A', Style{.bold = true});
    current.put(2, 0, U'B', Style{.bold = true});

    const auto operations = diff_frames(previous, current);
    REQUIRE(operations.size() == 3);
    CHECK(operations[0] == RenderOperation{RenderOperationKind::move_cursor, 1, 0, {}, {}});
    CHECK(operations[1].kind == RenderOperationKind::set_style);
    CHECK(operations[1].style.bold);
    CHECK(operations[2] == RenderOperation{RenderOperationKind::write, 0, 0, {}, "AB"});
}

TEST_CASE("frame diff rejects incompatible buffers", "[terminal][render]") {
    CHECK_THROWS_AS(diff_frames(ScreenBuffer{2, 2}, ScreenBuffer{3, 2}), std::invalid_argument);
}

TEST_CASE("screen buffer writes UTF-8 according to terminal cell width", "[terminal][unicode]") {
    ScreenBuffer buffer{6, 1};
    const int end = buffer.write_utf8(0, 0, "A界B");
    CHECK(end == 4);
    CHECK(buffer.cell(0, 0).glyph == U'A');
    CHECK(buffer.cell(1, 0).glyph == U'界');
    CHECK(buffer.cell(2, 0).continuation);
    CHECK(buffer.cell(3, 0).glyph == U'B');
    CHECK_THROWS_AS(buffer.write_utf8(0, 0, "\xFF"), vulpes::Error);
}
