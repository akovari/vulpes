#include "vulpes/ui/document_session.hpp"

#include "vulpes/core/actions.hpp"
#include "vulpes/core/error.hpp"

#include <string>

namespace vulpes::ui {
namespace {

void render_size_warning(terminal::ScreenBuffer& buffer, terminal::Size current, terminal::Size minimum) {
    const auto message = "Terminal is too small. Resize to at least " + std::to_string(minimum.width) + " x " +
                         std::to_string(minimum.height) + ". Esc or Ctrl+C exits.";
    const int row = current.height / 2;
    static_cast<void>(buffer.write_utf8(0, row, message, {.bold = true}));
}

[[nodiscard]] auto below_minimum(terminal::Size size, terminal::Size minimum) -> bool {
    return size.width < minimum.width || size.height < minimum.height;
}

} // namespace

void DocumentSession::run() {
    auto size = terminal_->size();
    if (size.width <= 0 || size.height <= 0)
        throw Error{ErrorCategory::terminal, "terminal reported an invalid size"};

    terminal::ScreenBuffer previous{size.width, size.height};
    terminal::ScreenBuffer current{size.width, size.height};
    core::ActionMap actions;

    for (;;) {
        const auto updated_size = terminal_->size();
        if (updated_size.width <= 0 || updated_size.height <= 0)
            throw Error{ErrorCategory::terminal, "terminal reported an invalid size"};
        if (updated_size.width != size.width || updated_size.height != size.height) {
            size = updated_size;
            previous = terminal::ScreenBuffer{size.width, size.height};
            current = terminal::ScreenBuffer{size.width, size.height};
        }

        current.clear();
        if (below_minimum(size, minimum_size_))
            render_size_warning(current, size, minimum_size_);
        else
            surface_->render(current, {0, 0, size.width, size.height});
        terminal_->present(previous, current);
        previous = current;

        const auto event = terminal_->read_event();
        const auto action = actions.action_for(event);
        if (below_minimum(size, minimum_size_)) {
            if (action == core::ActionId::application_back || action == core::ActionId::application_quit)
                return;
            continue;
        }
        if (surface_->handle(action, event) == DocumentResult::close)
            return;
    }
}

} // namespace vulpes::ui
