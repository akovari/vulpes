#include "vulpes/ui/button.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <utility>

namespace vulpes::ui {

Button::Button(std::string label) : label_{std::move(label)} {
}

auto Button::measure_width() const -> int {
    return terminal::text_width(label_) + 4;
}

void Button::render(terminal::ScreenBuffer& buffer, Rect bounds, bool focused, terminal::Style style) const {
    if (bounds.width <= 0 || bounds.height <= 0 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height())
        return;

    style.reverse = focused;
    const auto text = terminal::truncate_utf8("[ " + label_ + " ]", bounds.width);
    const auto end = buffer.write_utf8(bounds.x, bounds.y, text, style);
    for (int column = end; column < bounds.x + bounds.width; ++column)
        buffer.put(column, bounds.y, U' ', style);
}

} // namespace vulpes::ui
