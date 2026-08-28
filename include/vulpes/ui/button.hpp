#pragma once

#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"

#include <string>
#include <string_view>

namespace vulpes::ui {

// A semantic button renderer. Its owner decides which action activation means,
// keeping action dispatch separate from terminal presentation.
class Button {
  public:
    explicit Button(std::string label);

    [[nodiscard]] auto label() const noexcept -> std::string_view { return label_; }
    [[nodiscard]] auto measure_width() const -> int;
    void render(terminal::ScreenBuffer& buffer, Rect bounds, bool focused, terminal::Style style = {}) const;

  private:
    std::string label_;
};

} // namespace vulpes::ui
