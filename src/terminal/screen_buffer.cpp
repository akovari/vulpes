#include "vulpes/terminal/screen_buffer.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <utf8proc.h>

#include <algorithm>
#include <stdexcept>

namespace vulpes::terminal {

ScreenBuffer::ScreenBuffer(int width, int height)
    : width_{width}, height_{height}, cells_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
    if (width <= 0 || height <= 0) throw std::invalid_argument{"screen dimensions must be positive"};
}

auto ScreenBuffer::index(int x, int y) const -> std::size_t {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) throw std::out_of_range{"screen cell is out of range"};
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

auto ScreenBuffer::cell(int x, int y) const -> const Cell& { return cells_.at(index(x, y)); }
void ScreenBuffer::put(int x, int y, char32_t glyph, Style style) { cells_.at(index(x, y)) = Cell{glyph, style}; }

auto ScreenBuffer::write_utf8(int x, int y, std::string_view text, Style style) -> int {
    if (y < 0 || y >= height_) throw std::out_of_range{"screen row is out of range"};
    if (x < 0) throw std::out_of_range{"screen column is out of range"};
    const auto* cursor = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    auto remaining = static_cast<utf8proc_ssize_t>(text.size());
    int column = x;
    while (remaining > 0 && column < width_) {
        utf8proc_int32_t code_point{};
        const auto consumed = utf8proc_iterate(cursor, remaining, &code_point);
        if (consumed < 0) throw Error{ErrorCategory::terminal, "invalid UTF-8 text"};
        cursor += consumed;
        remaining -= consumed;
        const int width = cell_width(static_cast<char32_t>(code_point));
        if (width <= 0) continue;
        if (width > 1 && column + width > width_) break;
        put(column, y, static_cast<char32_t>(code_point), style);
        for (int offset = 1; offset < width; ++offset) cells_.at(index(column + offset, y)) = Cell{U' ', style, true};
        column += width;
    }
    return column;
}
void ScreenBuffer::clear(Cell fill) { std::ranges::fill(cells_, fill); }

} // namespace vulpes::terminal
