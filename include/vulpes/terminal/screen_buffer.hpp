#pragma once

#include <cstdint>
#include <vector>

namespace vulpes::terminal {

struct Color { std::uint8_t red{}, green{}, blue{}; auto operator==(const Color&) const -> bool = default; };

struct Style {
    Color foreground{255, 255, 255};
    Color background{0, 0, 0};
    bool bold{false};
    bool underline{false};
    bool reverse{false};
    auto operator==(const Style&) const -> bool = default;
};

struct Cell {
    char32_t glyph{U' '};
    Style style{};
    auto operator==(const Cell&) const -> bool = default;
};

class ScreenBuffer {
public:
    ScreenBuffer(int width, int height);
    [[nodiscard]] auto width() const noexcept -> int { return width_; }
    [[nodiscard]] auto height() const noexcept -> int { return height_; }
    [[nodiscard]] auto cell(int x, int y) const -> const Cell&;
    void put(int x, int y, char32_t glyph, Style style = {});
    void clear(Cell fill = {});

private:
    [[nodiscard]] auto index(int x, int y) const -> std::size_t;
    int width_;
    int height_;
    std::vector<Cell> cells_;
};

} // namespace vulpes::terminal

