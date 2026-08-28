#include "vulpes/ui/terminal_warning.hpp"

namespace vulpes::ui {

void render_terminal_warning(terminal::ScreenBuffer& buffer, terminal::Size size, std::string_view message) {
    const int row = size.height / 2;
    static_cast<void>(buffer.write_utf8(0, row, message, {.bold = true}));
}

} // namespace vulpes::ui
