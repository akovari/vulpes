#include "vulpes/ui/terminal_warning.hpp"

#include "vulpes/ui/container.hpp"
#include "vulpes/ui/label.hpp"

#include <string>

namespace vulpes::ui {

void render_terminal_warning(terminal::ScreenBuffer& buffer, terminal::Size size, std::string_view message) {
    Label warning{std::string{message}, Alignment::start, Alignment::center, {.bold = true}};
    Container host{Axis::vertical};
    host.add(warning, 1);
    host.layout({.width = size.width, .height = size.height});
    host.render(buffer);
}

} // namespace vulpes::ui
