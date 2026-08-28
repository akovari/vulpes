#pragma once

#include "vulpes/core/clipboard.hpp"
#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/text_editing.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace vulpes::ui {

enum class LineEditResult { unchanged, cursor_moved, changed };
struct LineViewport {
    std::string text;
    int cursor_column{};
    bool clipped_left{false};
    bool clipped_right{false};
    std::size_t first_offset{};
    std::optional<std::pair<std::size_t, std::size_t>> selection;
};

// A semantic, single-line UTF-8 editor. The cursor is stored as a byte offset
// but always remains on a code-point boundary. Rendering follows the cursor
// horizontally without exposing a terminal cursor to application code.
class LineEditor {
  public:
    explicit LineEditor(std::string text = {}, TextEditorOptions options = {});

    [[nodiscard]] auto text() const noexcept -> std::string_view { return text_; }
    [[nodiscard]] auto cursor_offset() const noexcept -> std::size_t { return cursor_; }
    [[nodiscard]] auto selection() const noexcept -> std::optional<std::pair<std::size_t, std::size_t>>;
    [[nodiscard]] auto selected_text() const -> std::string;
    [[nodiscard]] auto mode() const noexcept -> TextEditMode { return mode_; }
    void set_text(std::string text);
    [[nodiscard]] auto handle(const terminal::KeyEvent& event, core::Clipboard* clipboard = nullptr) -> LineEditResult;
    [[nodiscard]] auto handle(const terminal::InputEvent& event, core::Clipboard* clipboard = nullptr)
        -> LineEditResult;
    [[nodiscard]] auto viewport(int width, bool follow_cursor = true) const -> LineViewport;
    void render(terminal::ScreenBuffer& buffer, Rect bounds, terminal::Style style, bool focused) const;

  private:
    [[nodiscard]] auto previous_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto next_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto previous_word_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto next_word_offset() const noexcept -> std::size_t;
    void move_to(std::size_t offset, bool selecting) noexcept;
    [[nodiscard]] auto erase_selection() -> bool;
    [[nodiscard]] auto insert(std::string_view text) -> bool;

    std::string text_;
    std::size_t cursor_{};
    std::optional<std::size_t> selection_anchor_;
    TextEditMode mode_{TextEditMode::insert};
    bool allow_mode_toggle_{true};
};

} // namespace vulpes::ui
