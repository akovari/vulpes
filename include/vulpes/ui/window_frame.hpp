#pragma once

#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"

#include <string_view>

namespace vulpes::ui {

// An opaque terminal window frame. It provides deterministic border and title
// rendering while leaving layout and input policy with its semantic owner.
class WindowFrame {
  public:
    [[nodiscard]] static auto fits(const terminal::ScreenBuffer& buffer, Rect bounds, int minimum_width,
                                   int minimum_height) noexcept -> bool;
    [[nodiscard]] static auto content_bounds(Rect bounds) noexcept -> Rect;
    static void render(terminal::ScreenBuffer& buffer, Rect bounds, std::string_view title, terminal::Style style = {},
                       terminal::Style title_style = {});
};

} // namespace vulpes::ui
