#include "vulpes/core/error.hpp"
#include "vulpes/terminal/ansi_encoder.hpp"
#include "vulpes/terminal/test_terminal.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::terminal;

TEST_CASE("ANSI encoding translates semantic rendering operations", "[terminal][ansi]") {
    const std::vector<RenderOperation> operations{
        {RenderOperationKind::move_cursor, 2, 3, {}, {}},
        {RenderOperationKind::set_style,
         0,
         0,
         Style{.foreground = {1, 2, 3}, .background = {4, 5, 6}, .bold = true},
         {}},
        {RenderOperationKind::write, 0, 0, {}, "Hi"},
    };
    CHECK(encode_ansi(operations) == "\x1B[4;3H\x1B[38;2;1;2;3;48;2;4;5;6;1mHi");
    CHECK(ansi_reset() == "\x1B[0m");
}

TEST_CASE("test terminal queues normalized input and captures frames", "[terminal][fake]") {
    TestTerminal terminal{{80, 25}};
    terminal.enqueue(KeyEvent{.key = Key::f2});
    terminal.enqueue(ResizeEvent{.width = 100, .height = 40});
    CHECK(std::get<KeyEvent>(terminal.read_event()).key == Key::f2);
    CHECK(std::get<ResizeEvent>(terminal.read_event()).width == 100);
    CHECK_THROWS_AS(terminal.read_event(), vulpes::Error);

    ScreenBuffer before{80, 25};
    ScreenBuffer after{80, 25};
    after.put(0, 0, U'V');
    terminal.present(before, after);
    REQUIRE(terminal.frames().size() == 1);
    CHECK(terminal.frames().front().cell(0, 0).glyph == U'V');
}
