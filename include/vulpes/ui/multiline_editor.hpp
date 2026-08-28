#pragma once

#include "vulpes/core/clipboard.hpp"
#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/text_editing.hpp"

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
    std::optional<std::pair<int, int>> selection_columns;
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
    explicit MultilineEditor(std::string text = {}, TextEditorOptions options = {});

    [[nodiscard]] auto text() const noexcept -> std::string_view { return text_; }
    [[nodiscard]] auto cursor_offset() const noexcept -> std::size_t { return cursor_; }
    [[nodiscard]] auto selection() const noexcept -> std::optional<std::pair<std::size_t, std::size_t>>;
    [[nodiscard]] auto selected_text() const -> std::string;
    [[nodiscard]] auto mode() const noexcept -> TextEditMode { return mode_; }
    [[nodiscard]] auto can_undo() const noexcept -> bool { return !undo_.empty(); }
    [[nodiscard]] auto can_redo() const noexcept -> bool { return !redo_.empty(); }
    void set_text(std::string text);
    [[nodiscard]] auto handle(const terminal::KeyEvent& event, core::Clipboard* clipboard = nullptr)
        -> MultilineEditResult;
    [[nodiscard]] auto handle(const terminal::InputEvent& event, core::Clipboard* clipboard = nullptr)
        -> MultilineEditResult;
    [[nodiscard]] auto viewport(int width, int height) -> MultilineViewport;

  private:
    [[nodiscard]] auto previous_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto next_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto previous_word_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto next_word_offset() const noexcept -> std::size_t;
    [[nodiscard]] auto move_vertical(int line_delta, bool selecting) -> MultilineEditResult;
    void move_to(std::size_t offset, bool selecting) noexcept;
    [[nodiscard]] auto erase_selection() -> bool;
    [[nodiscard]] auto insert(std::string_view text) -> bool;
    void save_undo();
    [[nodiscard]] auto undo() -> bool;
    [[nodiscard]] auto redo() -> bool;
    void reset_preferred_column() noexcept { preferred_column_.reset(); }

    struct Snapshot {
        std::string text;
        std::size_t cursor{};
        std::optional<std::size_t> selection_anchor;
    };

    std::string text_;
    std::size_t cursor_{};
    std::optional<std::size_t> selection_anchor_;
    std::optional<int> preferred_column_;
    std::size_t first_visible_line_{};
    int horizontal_offset_{};
    int last_view_height_{1};
    TextEditMode mode_{TextEditMode::insert};
    bool allow_mode_toggle_{true};
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
    std::size_t undo_bytes_{};
    static constexpr std::size_t maximum_undo_states_{100};
    static constexpr std::size_t maximum_undo_bytes_{4U * 1024U * 1024U};
};

} // namespace vulpes::ui
