#pragma once

#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/theme.hpp"

#include <string_view>

namespace vulpes::ui {

struct WindowFrameAppearance {
    terminal::Style content;
    terminal::Style border;
    terminal::Style title;
    terminal::Style shadow;
    bool drop_shadow{false};
};

[[nodiscard]] auto window_frame_appearance(const Theme& theme, bool drop_shadow = false) -> WindowFrameAppearance;

// An opaque terminal window frame. It provides deterministic border and title
// rendering while leaving layout and input policy with its semantic owner.
class WindowFrame {
  public:
    [[nodiscard]] static auto fits(const terminal::ScreenBuffer& buffer, Rect bounds, int minimum_width,
                                   int minimum_height) noexcept -> bool;
    [[nodiscard]] static auto content_bounds(Rect bounds) noexcept -> Rect;
    static void render(terminal::ScreenBuffer& buffer, Rect bounds, std::string_view title,
                       const WindowFrameAppearance& appearance = {});
};

} // namespace vulpes::ui
