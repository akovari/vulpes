#include "vulpes/ui/focus_ring.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("focus ring skips unavailable controls and wraps predictably", "[ui][focus]") {
    vulpes::ui::FocusRing focus{{false, true, false, true}};

    REQUIRE(focus.current());
    CHECK(*focus.current() == 1);
    CHECK(focus.move(1));
    CHECK(*focus.current() == 3);
    CHECK(focus.move(1));
    CHECK(*focus.current() == 1);
    CHECK(focus.move(-1));
    CHECK(*focus.current() == 3);
    CHECK_FALSE(focus.select(2));
    CHECK(focus.select(1));
    CHECK(*focus.current() == 1);
}

TEST_CASE("focus ring reports no focus when every control is unavailable", "[ui][focus]") {
    vulpes::ui::FocusRing focus{{false, false}};

    CHECK_FALSE(focus.current());
    CHECK_FALSE(focus.move(1));
    CHECK_FALSE(focus.select(0));
}
