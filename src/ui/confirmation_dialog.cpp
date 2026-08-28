#include "vulpes/ui/confirmation_dialog.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>

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
    : title_{std::move(title)}, message_{std::move(message)}, confirm_label_{std::move(confirm_label)},
      cancel_label_{std::move(cancel_label)}, instructions_{std::move(instructions)}, button_focus_{{true, true}} {
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
    if (bounds.width < 24 || bounds.height < 6 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height())
        return;
    const int interior = bounds.width - 2;
    buffer.put(bounds.x, bounds.y, U'+');
    for (int column = 0; column < interior; ++column)
        buffer.put(bounds.x + 1 + column, bounds.y, U'-');
    buffer.put(bounds.x + bounds.width - 1, bounds.y, U'+');
    write_padded(buffer, bounds.x + 2, bounds.y, interior - 2, title_);

    for (int row = 1; row < bounds.height - 1; ++row) {
        const int y = bounds.y + row;
        buffer.put(bounds.x, y, U'|');
        buffer.put(bounds.x + bounds.width - 1, y, U'|');
    }
    write_padded(buffer, bounds.x + 1, bounds.y + 1, interior, message_);
    const int midpoint = interior / 2;
    write_padded(buffer, bounds.x + 1, bounds.y + 3, midpoint, "[ " + confirm_label_ + " ]", {.reverse = confirmed()});
    write_padded(buffer, bounds.x + 1 + midpoint, bounds.y + 3, interior - midpoint, "[ " + cancel_label_ + " ]",
                 {.reverse = !confirmed()});
    write_padded(buffer, bounds.x + 1, bounds.y + 4, interior, instructions_);

    const int bottom = bounds.y + bounds.height - 1;
    buffer.put(bounds.x, bottom, U'+');
    for (int column = 0; column < interior; ++column)
        buffer.put(bounds.x + 1 + column, bottom, U'-');
    buffer.put(bounds.x + bounds.width - 1, bottom, U'+');
}

} // namespace vulpes::ui
