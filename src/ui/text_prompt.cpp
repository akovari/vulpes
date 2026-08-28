#include "vulpes/ui/text_prompt.hpp"

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

TextPrompt::TextPrompt(std::string label, std::string instructions, std::string initial_value, const Theme& theme,
                       core::Clipboard* clipboard)
    : label_{std::move(label)}, instructions_{std::move(instructions)}, editor_{std::move(initial_value)},
      theme_{&theme}, clipboard_{clipboard} {
}

void TextPrompt::set_error(std::string message) {
    error_ = std::move(message);
}

auto TextPrompt::handle(const terminal::InputEvent& event) -> PromptResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (std::holds_alternative<terminal::ResizeEvent>(event))
        return PromptResult::redraw;
    if (key != nullptr && key->key == terminal::Key::escape)
        return PromptResult::cancelled;
    if (key != nullptr && key->key == terminal::Key::enter)
        return PromptResult::submitted;
    const auto result = editor_.handle(event, clipboard_);
    if (result != LineEditResult::unchanged) {
        if (result == LineEditResult::changed)
            error_.clear();
        return PromptResult::redraw;
    }
    return PromptResult::unchanged;
}

void TextPrompt::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 20, 5))
        return;
    const int interior = bounds.width - 2;
    WindowFrame::render(buffer, bounds, label_, window_frame_appearance(*theme_, true));
    write_padded(buffer, bounds.x + 1, bounds.y + 1, interior, "", theme_->style(ThemeRole::input_focus));
    static_cast<void>(buffer.write_utf8(bounds.x + 1, bounds.y + 1, "> ", theme_->style(ThemeRole::input_focus)));
    editor_.render(buffer, {bounds.x + 3, bounds.y + 1, interior - 2, 1}, theme_->style(ThemeRole::input_focus), true);
    write_padded(buffer, bounds.x + 1, bounds.y + 2, interior, error_.empty() ? instructions_ : error_,
                 theme_->style(error_.empty() ? ThemeRole::muted_text : ThemeRole::error));
}

} // namespace vulpes::ui
