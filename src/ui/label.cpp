#include "vulpes/ui/label.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <utility>

namespace vulpes::ui {
namespace {

[[nodiscard]] auto aligned_offset(int available, int occupied, Alignment alignment) noexcept -> int {
    const int remaining = available - occupied;
    switch (alignment) {
    case Alignment::center:
        return remaining / 2;
    case Alignment::end:
        return remaining;
    case Alignment::start:
        return 0;
    }
    return 0;
}

} // namespace

Label::Label(std::string text, Alignment horizontal_alignment, Alignment vertical_alignment, terminal::Style style)
    : text_{std::move(text)}, horizontal_alignment_{horizontal_alignment}, vertical_alignment_{vertical_alignment},
      style_{style} {
}

void Label::set_text(std::string text) {
    text_ = std::move(text);
}

auto Label::measure(Constraints constraints) const -> Extent {
    return constraints.constrain({.width = terminal::text_width(text_), .height = 1});
}

void Label::render(terminal::ScreenBuffer& buffer) const {
    const auto area = bounds();
    if (area.width <= 0 || area.height <= 0 || area.x < 0 || area.y < 0 || area.x + area.width > buffer.width() ||
        area.y + area.height > buffer.height())
        return;

    const auto clipped = terminal::truncate_utf8(text_, area.width);
    const int text_width = terminal::text_width(clipped);
    const int x = area.x + aligned_offset(area.width, text_width, horizontal_alignment_);
    const int y = area.y + aligned_offset(area.height, 1, vertical_alignment_);
    static_cast<void>(buffer.write_utf8(x, y, clipped, style_));
}

} // namespace vulpes::ui
