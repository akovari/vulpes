#include "vulpes/terminal/test_terminal.hpp"

#include "vulpes/core/error.hpp"

namespace vulpes::terminal {

auto TestTerminal::read_event() -> InputEvent {
    if (events_.empty())
        throw Error{ErrorCategory::terminal, "test terminal has no queued input event"};
    auto event = events_.front();
    events_.pop_front();
    return event;
}

void TestTerminal::present(const ScreenBuffer& previous, const ScreenBuffer& current) {
    if (previous.width() != current.width() || previous.height() != current.height()) {
        throw Error{ErrorCategory::terminal, "test terminal received incompatible screen buffers"};
    }
    frames_.push_back(current);
}

void TestTerminal::enqueue(InputEvent event) {
    events_.push_back(std::move(event));
}

} // namespace vulpes::terminal
