#pragma once

#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/layout.hpp"

namespace vulpes::ui {

// Base contract for lightweight semantic layout nodes. Widgets own semantic
// state; layout stores only their most recently assigned logical bounds.
class Widget {
  public:
    virtual ~Widget();

    [[nodiscard]] virtual auto measure(Constraints constraints = {}) const -> Extent = 0;
    virtual void layout(Rect bounds);
    virtual void render(terminal::ScreenBuffer& buffer) const = 0;

    [[nodiscard]] auto bounds() const noexcept -> Rect { return bounds_; }

  private:
    Rect bounds_{};
};

} // namespace vulpes::ui
