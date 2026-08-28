#pragma once

#include "vulpes/terminal/terminal.hpp"

#include <string_view>

namespace vulpes::ui {

// Renders a host-level warning without coupling documents or workspace chrome
// to any terminal escape-sequence implementation.
void render_terminal_warning(terminal::ScreenBuffer& buffer, terminal::Size size, std::string_view message);

} // namespace vulpes::ui
