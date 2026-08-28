#pragma once

#include "vulpes/ui/widget.hpp"

#include <string>
#include <string_view>

namespace vulpes::ui {

class Label final : public Widget {
  public:
    explicit Label(std::string text, Alignment horizontal_alignment = Alignment::start,
                   Alignment vertical_alignment = Alignment::start, terminal::Style style = {});

    [[nodiscard]] auto text() const noexcept -> std::string_view { return text_; }
    void set_text(std::string text);
    void set_style(terminal::Style style) noexcept { style_ = style; }

    [[nodiscard]] auto measure(Constraints constraints = {}) const -> Extent override;
    void render(terminal::ScreenBuffer& buffer) const override;

  private:
    std::string text_;
    Alignment horizontal_alignment_;
    Alignment vertical_alignment_;
    terminal::Style style_;
};

} // namespace vulpes::ui
