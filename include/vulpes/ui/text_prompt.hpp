#pragma once

#include "vulpes/core/clipboard.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/line_editor.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <string>
#include <string_view>

namespace vulpes::ui {

enum class PromptResult { unchanged, redraw, submitted, cancelled };

// A small, reusable semantic text entry overlay. Callers own the meaning of the
// submitted text, keeping command, filter, and search parsing out of widgets.
class TextPrompt {
  public:
    TextPrompt(std::string label, std::string instructions, std::string initial_value = {},
               const Theme& theme = ui::theme(ThemeName::midnight), core::Clipboard* clipboard = nullptr);

    [[nodiscard]] auto value() const noexcept -> std::string_view { return editor_.text(); }
    [[nodiscard]] auto cursor_offset() const noexcept -> std::size_t { return editor_.cursor_offset(); }
    void set_error(std::string message);
    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> PromptResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    std::string label_;
    std::string instructions_;
    LineEditor editor_;
    std::string error_;
    const Theme* theme_;
    core::Clipboard* clipboard_;
};

} // namespace vulpes::ui
