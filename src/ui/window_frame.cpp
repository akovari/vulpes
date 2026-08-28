#include "vulpes/ui/window_frame.hpp"

#include "vulpes/terminal/unicode.hpp"

namespace vulpes::ui {

auto window_frame_appearance(const Theme& theme, bool drop_shadow) -> WindowFrameAppearance {
    return {.content = theme.style(ThemeRole::text),
            .border = theme.style(ThemeRole::border),
            .title = theme.style(ThemeRole::title),
            .shadow = theme.style(ThemeRole::shadow),
            .drop_shadow = drop_shadow};
}

auto WindowFrame::fits(const terminal::ScreenBuffer& buffer, Rect bounds, int minimum_width,
                       int minimum_height) noexcept -> bool {
    return minimum_width >= 2 && minimum_height >= 2 && bounds.width >= minimum_width &&
           bounds.height >= minimum_height && bounds.x >= 0 && bounds.y >= 0 &&
           bounds.x + bounds.width <= buffer.width() && bounds.y + bounds.height <= buffer.height();
}

auto WindowFrame::content_bounds(Rect bounds) noexcept -> Rect {
    return {.x = bounds.x + 1, .y = bounds.y + 1, .width = bounds.width - 2, .height = bounds.height - 2};
}

void WindowFrame::render(terminal::ScreenBuffer& buffer, Rect bounds, std::string_view title,
                         const WindowFrameAppearance& appearance) {
    if (!fits(buffer, bounds, 2, 2))
        return;

    const auto content = content_bounds(bounds);
    if (appearance.drop_shadow) {
        const int shadow_x = bounds.x + bounds.width;
        const int shadow_y = bounds.y + bounds.height;
        if (shadow_x < buffer.width()) {
            for (int row = bounds.y + 1; row < shadow_y && row < buffer.height(); ++row)
                buffer.put(shadow_x, row, U' ', appearance.shadow);
        }
        if (shadow_y < buffer.height()) {
            for (int column = bounds.x + 1; column <= shadow_x && column < buffer.width(); ++column)
                buffer.put(column, shadow_y, U' ', appearance.shadow);
        }
    }
    for (int row = content.y; row < content.y + content.height; ++row) {
        for (int column = content.x; column < content.x + content.width; ++column)
            buffer.put(column, row, U' ', appearance.content);
    }
    for (int column = content.x; column < content.x + content.width; ++column) {
        buffer.put(column, bounds.y, U'─', appearance.border);
        buffer.put(column, bounds.y + bounds.height - 1, U'─', appearance.border);
    }
    for (int row = content.y; row < content.y + content.height; ++row) {
        buffer.put(bounds.x, row, U'│', appearance.border);
        buffer.put(bounds.x + bounds.width - 1, row, U'│', appearance.border);
    }
    buffer.put(bounds.x, bounds.y, U'┌', appearance.border);
    buffer.put(bounds.x + bounds.width - 1, bounds.y, U'┐', appearance.border);
    buffer.put(bounds.x, bounds.y + bounds.height - 1, U'└', appearance.border);
    buffer.put(bounds.x + bounds.width - 1, bounds.y + bounds.height - 1, U'┘', appearance.border);

    const int title_width = bounds.width - 4;
    if (title_width > 0 && !title.empty()) {
        const auto decorated_title = " " + std::string{title} + " ";
        static_cast<void>(buffer.write_utf8(bounds.x + 2, bounds.y,
                                            terminal::truncate_utf8(decorated_title, title_width), appearance.title));
    }
}

} // namespace vulpes::ui
