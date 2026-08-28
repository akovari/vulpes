#include "vulpes/ui/confirmation_dialog.hpp"

#include "vulpes/terminal/unicode.hpp"

namespace vulpes::ui {
namespace {

void write_padded(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text,
                  terminal::Style style = {}) {
    const auto clipped = terminal::truncate_utf8(text, width);
    const auto end = buffer.write_utf8(x, y, clipped, style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

ConfirmationDialog::ConfirmationDialog(std::string title, std::string message, std::string confirm_label,
                                       std::string cancel_label, std::string instructions)
    : title_{std::move(title)}, message_{std::move(message)}, instructions_{std::move(instructions)},
      confirm_button_{std::move(confirm_label)}, cancel_button_{std::move(cancel_label)}, button_focus_{{true, true}} {
    static_cast<void>(button_focus_.select(1));
}

auto ConfirmationDialog::handle(const terminal::InputEvent& event) -> ConfirmationResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key == nullptr)
        return ConfirmationResult::redraw;
    if (key->key == terminal::Key::escape)
        return ConfirmationResult::cancelled;
    if (key->key == terminal::Key::left || (key->key == terminal::Key::tab && key->shift)) {
        static_cast<void>(button_focus_.move(-1));
        return ConfirmationResult::redraw;
    }
    if (key->key == terminal::Key::right || key->key == terminal::Key::tab) {
        static_cast<void>(button_focus_.move(1));
        return ConfirmationResult::redraw;
    }
    if (key->key == terminal::Key::character && !key->ctrl && !key->alt) {
        if (key->character == U'y' || key->character == U'Y') {
            static_cast<void>(button_focus_.select(0));
            return ConfirmationResult::redraw;
        }
        if (key->character == U'n' || key->character == U'N') {
            static_cast<void>(button_focus_.select(1));
            return ConfirmationResult::redraw;
        }
    }
    if (key->key == terminal::Key::enter)
        return confirmed() ? ConfirmationResult::confirmed : ConfirmationResult::cancelled;
    return ConfirmationResult::unchanged;
}

void ConfirmationDialog::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 24, 6))
        return;
    const int interior = bounds.width - 2;
    WindowFrame::render(buffer, bounds, title_);
    write_padded(buffer, bounds.x + 1, bounds.y + 1, interior, message_);
    const int midpoint = interior / 2;
    confirm_button_.render(buffer, {bounds.x + 1, bounds.y + 3, midpoint, 1}, confirmed());
    cancel_button_.render(buffer, {bounds.x + 1 + midpoint, bounds.y + 3, interior - midpoint, 1}, !confirmed());
    write_padded(buffer, bounds.x + 1, bounds.y + 4, interior, instructions_);
}

} // namespace vulpes::ui
