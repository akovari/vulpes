#pragma once

#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/button.hpp"
#include "vulpes/ui/focus_ring.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <string>

namespace vulpes::ui {

enum class ConfirmationResult { unchanged, redraw, confirmed, cancelled };

// A reusable destructive-action dialog. It deliberately defaults to cancel so
// an accidental Enter can never confirm an operation.
class ConfirmationDialog {
  public:
    ConfirmationDialog(std::string title, std::string message, std::string confirm_label, std::string cancel_label,
                       std::string instructions);

    [[nodiscard]] auto confirmed() const noexcept -> bool { return button_focus_.current().value_or(1) == 0; }
    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> ConfirmationResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    std::string title_;
    std::string message_;
    std::string instructions_;
    Button confirm_button_;
    Button cancel_button_;
    FocusRing button_focus_;
};

} // namespace vulpes::ui
