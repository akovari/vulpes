#include "vulpes/ui/window_stack.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <variant>

TEST_CASE("semantic window stack routes and dismisses only its top layer", "[ui][window][stack]") {
    using Content = std::variant<int, std::string>;
    vulpes::ui::WindowStack<Content> windows;
    windows.push({.id = "form", .title = "Customer", .kind = vulpes::ui::WindowLayerKind::form}, Content{42});
    windows.push({.id = "lookup", .title = "Choose customer", .kind = vulpes::ui::WindowLayerKind::lookup},
                 Content{std::string{"search"}});

    REQUIRE(windows.size() == 2);
    CHECK(windows.top().descriptor.id == "lookup");
    CHECK(std::get<std::string>(windows.top().content) == "search");
    windows.top().descriptor.dirty = true;
    CHECK(windows.top().descriptor.dirty);

    REQUIRE(windows.pop());
    CHECK(windows.top().descriptor.id == "form");
    CHECK(std::get<int>(windows.top().content) == 42);
    REQUIRE(windows.pop());
    CHECK(windows.empty());
    CHECK_FALSE(windows.pop());
}
