#pragma once

#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/screen_buffer.hpp"

#include <string>

namespace vulpes::ui {

struct Rect {
    int x{};
    int y{};
    int width{};
    int height{};
};

class Grid {
  public:
    Grid(const model::Dataset& dataset, std::string title);
    [[nodiscard]] auto move_left() -> bool;
    [[nodiscard]] auto move_right() -> bool;
    [[nodiscard]] auto selected_column_index() const noexcept -> std::size_t { return selected_column_; }
    [[nodiscard]] auto first_visible_column_index() const noexcept -> std::size_t { return first_visible_column_; }
    void render(terminal::ScreenBuffer& buffer, Rect bounds);

  private:
    const model::Dataset* dataset_;
    std::string title_;
    std::size_t selected_column_{};
    std::size_t first_visible_column_{};
};

} // namespace vulpes::ui
