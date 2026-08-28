#include "vulpes/ui/status_bar.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>

namespace vulpes::ui {
namespace {

void write(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text, terminal::Style style) {
    if (width <= 0)
        return;
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

void StatusBar::render(terminal::ScreenBuffer& buffer, Rect bounds, const Theme& theme, std::string_view message,
                       std::span<const ShortcutHint> shortcuts) {
    if (bounds.width < 1 || bounds.height < 1 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height())
        return;

    const auto& background = theme.style(ThemeRole::status_bar);
    write(buffer, bounds.x, bounds.y, bounds.width, "", background);
    const int end = bounds.x + bounds.width - 1;
    int x = bounds.x + 1;
    if (!message.empty()) {
        write(buffer, x, bounds.y, std::max(0, end - x), message, background);
        return;
    }

    for (const auto& shortcut : shortcuts) {
        if (x >= end)
            return;
        const int key_width = std::min(terminal::text_width(shortcut.key), end - x);
        write(buffer, x, bounds.y, key_width, shortcut.key, theme.style(ThemeRole::status_bar_shortcut));
        x += key_width;
        if (x >= end)
            return;
        const int label_width = std::min(terminal::text_width(shortcut.label), end - x);
        write(buffer, x, bounds.y, label_width, shortcut.label, background);
        x += label_width;
    }
}

} // namespace vulpes::ui
