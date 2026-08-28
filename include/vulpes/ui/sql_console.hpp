#pragma once

#include "vulpes/core/clipboard.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/multiline_editor.hpp"
#include "vulpes/ui/theme.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vulpes::ui {

enum class SqlConsoleResult { unchanged, redraw, execute, cancelled };

// A terminal-independent multiline SQL editor. It owns only user interaction;
// the presentation layer executes submitted SQL through db::Database.
class SqlConsole {
  public:
    SqlConsole(std::string title, std::string instructions, const Theme& theme = ui::theme(ThemeName::midnight),
               core::Clipboard* clipboard = nullptr);

    [[nodiscard]] auto script() const noexcept -> std::string_view { return editor_.text(); }
    [[nodiscard]] auto cursor_offset() const noexcept -> std::size_t { return editor_.cursor_offset(); }
    [[nodiscard]] auto history_size() const noexcept -> std::size_t { return history_.size(); }
    void set_error(std::string message);
    void set_status(std::string message);
    void set_focused(bool focused) noexcept { focused_ = focused; }
    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> SqlConsoleResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds);

  private:
    std::string title_;
    std::string instructions_;
    MultilineEditor editor_;
    std::string status_;
    std::string error_;
    const Theme* theme_;
    bool focused_{true};
    core::Clipboard* clipboard_;
    std::vector<std::string> history_;
    std::optional<std::size_t> history_index_;
    std::string history_draft_;
    static constexpr std::size_t maximum_history_{100};

    void remember_script();
    [[nodiscard]] auto move_history(int direction) -> bool;
};

} // namespace vulpes::ui
