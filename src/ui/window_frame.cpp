#include "vulpes/ui/window_frame.hpp"

#include "vulpes/terminal/unicode.hpp"

namespace vulpes::ui {

auto WindowFrame::fits(const terminal::ScreenBuffer& buffer, Rect bounds, int minimum_width,
                       int minimum_height) noexcept -> bool {
    return minimum_width >= 2 && minimum_height >= 2 && bounds.width >= minimum_width &&
           bounds.height >= minimum_height && bounds.x >= 0 && bounds.y >= 0 &&
           bounds.x + bounds.width <= buffer.width() && bounds.y + bounds.height <= buffer.height();
}

auto WindowFrame::content_bounds(Rect bounds) noexcept -> Rect {
    return {.x = bounds.x + 1, .y = bounds.y + 1, .width = bounds.width - 2, .height = bounds.height - 2};
}

void WindowFrame::render(terminal::ScreenBuffer& buffer, Rect bounds, std::string_view title, terminal::Style style,
                         terminal::Style title_style) {
    if (!fits(buffer, bounds, 2, 2))
        return;

    const auto content = content_bounds(bounds);
    for (int row = content.y; row < content.y + content.height; ++row) {
        for (int column = content.x; column < content.x + content.width; ++column)
            buffer.put(column, row, U' ', style);
    }
    for (int column = content.x; column < content.x + content.width; ++column) {
        buffer.put(column, bounds.y, U'-', style);
        buffer.put(column, bounds.y + bounds.height - 1, U'-', style);
    }
    for (int row = content.y; row < content.y + content.height; ++row) {
        buffer.put(bounds.x, row, U'|', style);
        buffer.put(bounds.x + bounds.width - 1, row, U'|', style);
    }
    buffer.put(bounds.x, bounds.y, U'+', style);
    buffer.put(bounds.x + bounds.width - 1, bounds.y, U'+', style);
    buffer.put(bounds.x, bounds.y + bounds.height - 1, U'+', style);
    buffer.put(bounds.x + bounds.width - 1, bounds.y + bounds.height - 1, U'+', style);

    const int title_width = bounds.width - 3;
    if (title_width > 0)
        static_cast<void>(
            buffer.write_utf8(bounds.x + 2, bounds.y, terminal::truncate_utf8(title, title_width), title_style));
}

} // namespace vulpes::ui
