#include "vulpes/ui/layout.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout constraints normalize and clamp requested extents", "[ui][layout]") {
    const vulpes::ui::Constraints constraints{
        .minimum = {.width = 4, .height = 2},
        .maximum = {.width = 10, .height = 6},
    };

    CHECK(constraints.constrain({.width = 1, .height = 20}) == vulpes::ui::Extent{4, 6});
    CHECK(vulpes::ui::Constraints{.minimum = {.width = 5}, .maximum = {.width = 2}}.constrain({}) ==
          vulpes::ui::Extent{5, 0});
}
