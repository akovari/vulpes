#include "vulpes/terminal/cpp_terminal_adapter.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CPP-Terminal key adapter preserves semantic keys and modifiers", "[terminal][input]") {
    using vulpes::terminal::Key;
    using vulpes::terminal::normalize_cpp_terminal_key;

    constexpr std::int32_t unicode_limit{0x10FFFF};
    constexpr std::int32_t alt_modifier{1 << 22};
    constexpr std::int32_t ctrl_modifier{1 << 23};

    const auto escape = normalize_cpp_terminal_key(27);
    CHECK(escape.key == Key::escape);

    const auto left = normalize_cpp_terminal_key(unicode_limit + 1);
    CHECK(left.key == Key::left);

    const auto function = normalize_cpp_terminal_key(unicode_limit + 20);
    CHECK(function.key == Key::f10);

    const auto alt_file = normalize_cpp_terminal_key(alt_modifier | static_cast<std::int32_t>('f'));
    CHECK(alt_file.key == Key::character);
    CHECK(alt_file.character == U'f');
    CHECK(alt_file.alt);

    const auto quit = normalize_cpp_terminal_key(ctrl_modifier | static_cast<std::int32_t>('c'));
    CHECK(quit.key == Key::character);
    CHECK(quit.character == U'c');
    CHECK(quit.ctrl);

    const auto control_quit = normalize_cpp_terminal_key(3);
    CHECK(control_quit.key == Key::character);
    CHECK(control_quit.character == U'c');
    CHECK(control_quit.ctrl);
}
