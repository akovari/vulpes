#include "vulpes/terminal/ansi_input.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ANSI input decoder normalizes navigation and workflow keys", "[terminal][input]") {
    using vulpes::terminal::decode_ansi_key_sequence;
    using vulpes::terminal::Key;

    CHECK(decode_ansi_key_sequence("[A") == Key::up);
    CHECK(decode_ansi_key_sequence("[3~") == Key::delete_key);
    CHECK(decode_ansi_key_sequence("[5~") == Key::page_up);
    CHECK(decode_ansi_key_sequence("[15~") == Key::f5);
    CHECK(decode_ansi_key_sequence("[19~") == Key::f8);
    CHECK(decode_ansi_key_sequence("OQ") == Key::f2);
    CHECK(decode_ansi_key_sequence("[99~") == Key::unknown);
}
