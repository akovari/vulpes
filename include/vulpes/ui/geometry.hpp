#pragma once

namespace vulpes::ui {

struct Rect {
    int x{};
    int y{};
    int width{};
    int height{};
    auto operator==(const Rect&) const -> bool = default;
};

} // namespace vulpes::ui
