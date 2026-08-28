#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/label.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("label measures Unicode cells and aligns within assigned bounds", "[ui][label]") {
    vulpes::ui::Label label{"A界", vulpes::ui::Alignment::center, vulpes::ui::Alignment::center, {.bold = true}};
    vulpes::terminal::ScreenBuffer buffer{12, 5};

    CHECK(label.measure() == vulpes::ui::Extent{3, 1});
    CHECK(label.measure({.maximum = {.width = 2, .height = 1}}) == vulpes::ui::Extent{2, 1});
    label.layout({.x = 1, .y = 1, .width = 9, .height = 3});
    label.render(buffer);

    CHECK(buffer.cell(4, 2).glyph == U'A');
    CHECK(buffer.cell(5, 2).glyph == U'界');
    CHECK(buffer.cell(6, 2).continuation);
    CHECK(buffer.cell(4, 2).style.bold);
}

TEST_CASE("label updates semantic text without changing its layout contract", "[ui][label]") {
    vulpes::ui::Label label{"old"};
    label.set_text("new");
    CHECK(label.text() == "new");
    CHECK(label.measure() == vulpes::ui::Extent{3, 1});
}
