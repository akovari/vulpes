#include "vulpes/terminal/console_terminal.hpp"

#include "vulpes/terminal/ansi_encoder.hpp"
#include "vulpes/terminal/cpp_terminal_adapter.hpp"
#include "vulpes/terminal/frame_diff.hpp"

#include <cpp-terminal/input.hpp>
#include <cpp-terminal/screen.hpp>
#include <cpp-terminal/terminal_impl.hpp>
#include <cpp-terminal/terminal_initializer.hpp>
#include <iostream>
#include <memory>

namespace Term {
extern Terminal& terminal;
}

namespace vulpes::terminal {

class ConsoleTerminal::Implementation {
  public:
    Term::TerminalInitializer initializer;
};

ConsoleTerminal::ConsoleTerminal() : implementation_{std::make_unique<Implementation>()} {
    // Keep Ctrl+C as an application action so Vulpes can unwind modals and
    // restore the screen through normal RAII. CPP-Terminal also restores its
    // host state if the process is interrupted outside the event loop.
    Term::terminal.setOptions(Term::Option::ClearScreen, Term::Option::NoSignalKeys, Term::Option::NoCursor,
                              Term::Option::Raw);
}

ConsoleTerminal::~ConsoleTerminal() = default;

ConsoleTerminal::ConsoleTerminal(ConsoleTerminal&&) noexcept = default;
auto ConsoleTerminal::operator=(ConsoleTerminal&&) noexcept -> ConsoleTerminal& = default;

auto ConsoleTerminal::size() const -> Size {
    const auto screen = Term::screen_size();
    return {static_cast<int>(screen.columns()), static_cast<int>(screen.rows())};
}

auto ConsoleTerminal::read_event() -> InputEvent {
    for (;;) {
        const Term::Event event = Term::read_event();
        if (const auto* key = event.get_if_key())
            return normalize_cpp_terminal_key(static_cast<std::int32_t>(*key));
        if (const auto* screen = event.get_if_screen())
            return ResizeEvent{static_cast<int>(screen->columns()), static_cast<int>(screen->rows())};
    }
}

void ConsoleTerminal::present(const ScreenBuffer& previous, const ScreenBuffer& current) {
    std::cout << encode_ansi(diff_frames(previous, current)) << std::flush;
}

} // namespace vulpes::terminal
