#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/container.hpp"
#include "vulpes/ui/label.hpp"

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

TEST_CASE("linear container measures, spaces, and grows children", "[ui][layout][container]") {
    vulpes::ui::Label first{"AAA"};
    vulpes::ui::Label second{"B"};
    vulpes::ui::Container row{vulpes::ui::Axis::horizontal, 1};
    row.add(first);
    row.add(second, 1);

    CHECK(row.child_count() == 2);
    CHECK(row.measure() == vulpes::ui::Extent{5, 1});

    row.layout({.x = 1, .y = 2, .width = 10, .height = 2});
    CHECK(first.bounds() == vulpes::ui::Rect{1, 2, 3, 2});
    CHECK(second.bounds() == vulpes::ui::Rect{5, 2, 6, 2});

    vulpes::terminal::ScreenBuffer buffer{12, 5};
    row.render(buffer);
    CHECK(buffer.cell(1, 2).glyph == U'A');
    CHECK(buffer.cell(5, 2).glyph == U'B');
}

TEST_CASE("linear container preserves visible children while shrinking", "[ui][layout][container]") {
    vulpes::ui::Label first{"AAA"};
    vulpes::ui::Label second{"B"};
    vulpes::ui::Container row{vulpes::ui::Axis::horizontal, 1};
    row.add(first);
    row.add(second);

    row.layout({.width = 3, .height = 1});
    CHECK(first.bounds() == vulpes::ui::Rect{0, 0, 1, 1});
    CHECK(second.bounds() == vulpes::ui::Rect{2, 0, 1, 1});
}

TEST_CASE("linear container rejects invalid layout policy", "[ui][layout][container]") {
    CHECK_THROWS_AS((vulpes::ui::Container{vulpes::ui::Axis::vertical, -1}), std::invalid_argument);
    vulpes::ui::Container column{vulpes::ui::Axis::vertical};
    vulpes::ui::Label label{"value"};
    CHECK_THROWS_AS(column.add(label, -1), std::invalid_argument);
}
