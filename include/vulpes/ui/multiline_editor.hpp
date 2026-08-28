#pragma once

#include "vulpes/terminal/input.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vulpes::ui {

enum class MultilineEditResult { unchanged, cursor_moved, changed };

struct MultilineViewportLine {
    std::string text;
    bool clipped_left{false};
    bool clipped_right{false};
};

struct MultilineViewport {
    std::vector<MultilineViewportLine> lines;
    std::size_t first_line{};
    int cursor_row{};
    int cursor_column{};
    bool clipped_above{false};
    bool clipped_below{false};
};

// A terminal-independent UTF-8 text editor model. It owns logical cursor and
// viewport state, while its caller remains responsible for window chrome and
// drawing the returned display-cell slices.
class MultilineEditor {
  public:
    explicit MultilineEditor(std::string text = {});

    [[nodiscard]] auto text() const noexcept -> std::string_view { return text_; }
    [[nodiscard]] auto cursor_offset() const noexcept -> std::size_t { return cursor_; }
    void set_text(std::string text);
    [[nodiscard]] auto handle(const terminal::KeyEvent& event) -> MultilineEditResult;
    [[nodiscard]] auto viewport(int width, int height) -> MultilineViewport;

  private:
    [[nodiscard]] auto previous_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto next_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto move_vertical(int line_delta) -> MultilineEditResult;
    void reset_preferred_column() noexcept { preferred_column_.reset(); }

    std::string text_;
    std::size_t cursor_{};
    std::optional<int> preferred_column_;
    std::size_t first_visible_line_{};
    int horizontal_offset_{};
    int last_view_height_{1};
};

} // namespace vulpes::ui
