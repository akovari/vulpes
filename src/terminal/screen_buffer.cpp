#include "vulpes/terminal/screen_buffer.hpp"

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
void ScreenBuffer::clear(Cell fill) { std::ranges::fill(cells_, fill); }

} // namespace vulpes::terminal

