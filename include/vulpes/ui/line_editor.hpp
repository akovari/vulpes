#pragma once

#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace vulpes::ui {

enum class LineEditResult { unchanged, cursor_moved, changed };

struct LineViewport {
    std::string text;
    int cursor_column{};
    bool clipped_left{false};
    bool clipped_right{false};
};

// A semantic, single-line UTF-8 editor. The cursor is stored as a byte offset
// but always remains on a code-point boundary. Rendering follows the cursor
// horizontally without exposing a terminal cursor to application code.
class LineEditor {
  public:
    explicit LineEditor(std::string text = {});

    [[nodiscard]] auto text() const noexcept -> std::string_view { return text_; }
    [[nodiscard]] auto cursor_offset() const noexcept -> std::size_t { return cursor_; }
    void set_text(std::string text);
    [[nodiscard]] auto handle(const terminal::KeyEvent& event) -> LineEditResult;
    [[nodiscard]] auto viewport(int width, bool follow_cursor = true) const -> LineViewport;
    void render(terminal::ScreenBuffer& buffer, Rect bounds, terminal::Style style, bool focused) const;

  private:
    [[nodiscard]] auto previous_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto next_offset() const noexcept -> std::size_t;

    std::string text_;
    std::size_t cursor_{};
};

} // namespace vulpes::ui
