#include "vulpes/terminal/console_terminal.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/ansi_encoder.hpp"
#include "vulpes/terminal/capabilities.hpp"
#include "vulpes/terminal/cpp_terminal_adapter.hpp"
#include "vulpes/terminal/frame_diff.hpp"

#include <cpp-terminal/input.hpp>
#include <cpp-terminal/screen.hpp>
#include <cpp-terminal/terminal_impl.hpp>
#include <cpp-terminal/terminal_initializer.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace Term {
extern Terminal& terminal;
}

namespace vulpes::terminal {

class ConsoleTerminal::Implementation {
  public:
    Term::TerminalInitializer initializer;
};

ConsoleTerminal::ConsoleTerminal() {
    const auto issue = console_capability_issue(detect_console_capabilities());
    if (issue != ConsoleCapabilityIssue::none)
        throw Error{ErrorCategory::terminal, std::string{console_capability_message(issue)}};

    // Keep Ctrl+C as an application action so Vulpes can unwind modals and
    // restore the screen through normal RAII. CPP-Terminal also restores its
    // host state if the process is interrupted outside the event loop.
    try {
        auto implementation = std::make_unique<Implementation>();
        Term::terminal.setOptions(Term::Option::ClearScreen, Term::Option::NoSignalKeys, Term::Option::NoCursor,
                                  Term::Option::Raw);
        implementation_ = std::move(implementation);
    } catch (const std::exception& exception) {
        throw Error{ErrorCategory::terminal,
                    "unable to initialize interactive terminal: " + std::string{exception.what()}};
    }
}

ConsoleTerminal::~ConsoleTerminal() = default;

ConsoleTerminal::ConsoleTerminal(ConsoleTerminal&&) noexcept = default;
auto ConsoleTerminal::operator=(ConsoleTerminal&&) noexcept -> ConsoleTerminal& = default;

auto ConsoleTerminal::size() const -> Size {
    try {
        const auto screen = Term::screen_size();
        return {static_cast<int>(screen.columns()), static_cast<int>(screen.rows())};
    } catch (const std::exception& exception) {
        throw Error{ErrorCategory::terminal, "unable to read terminal size: " + std::string{exception.what()}};
    }
}

auto ConsoleTerminal::read_event() -> InputEvent {
    try {
        for (;;) {
            const Term::Event event = Term::read_event();
            if (const auto* key = event.get_if_key())
                return normalize_cpp_terminal_key(static_cast<std::int32_t>(*key));
            if (const auto* screen = event.get_if_screen())
                return ResizeEvent{static_cast<int>(screen->columns()), static_cast<int>(screen->rows())};
            if (const auto* paste = event.get_if_copy_paste())
                return PasteEvent{*paste};
        }
    } catch (const std::exception& exception) {
        throw Error{ErrorCategory::terminal, "unable to read terminal input: " + std::string{exception.what()}};
    }
}

void ConsoleTerminal::present(const ScreenBuffer& previous, const ScreenBuffer& current) {
    try {
        std::cout << encode_ansi(diff_frames(previous, current)) << std::flush;
        if (!std::cout)
            throw Error{ErrorCategory::terminal, "unable to write terminal output"};
    } catch (const Error&) {
        throw;
    } catch (const std::exception& exception) {
        throw Error{ErrorCategory::terminal, "unable to write terminal output: " + std::string{exception.what()}};
    }
}

} // namespace vulpes::terminal
