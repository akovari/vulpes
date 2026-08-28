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

void erase_last_code_point(std::string& text) {
    if (text.empty())
        return;
    std::size_t offset = text.size() - 1;
    while (offset > 0 && (static_cast<unsigned char>(text[offset]) & 0xC0U) == 0x80U)
        --offset;
    text.erase(offset);
}

} // namespace

TextPrompt::TextPrompt(std::string label, std::string instructions, std::string initial_value)
    : label_{std::move(label)}, instructions_{std::move(instructions)}, value_{std::move(initial_value)} {
}

void TextPrompt::set_error(std::string message) {
    error_ = std::move(message);
}

auto TextPrompt::handle(const terminal::InputEvent& event) -> PromptResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key == nullptr)
        return PromptResult::redraw;
    if (key->key == terminal::Key::escape)
        return PromptResult::cancelled;
    if (key->key == terminal::Key::enter)
        return PromptResult::submitted;
    if (key->key == terminal::Key::backspace) {
        erase_last_code_point(value_);
        error_.clear();
        return PromptResult::redraw;
    }
    if (key->key == terminal::Key::character && !key->ctrl && !key->alt) {
        value_ += terminal::encode_utf8(key->character);
        error_.clear();
        return PromptResult::redraw;
    }
    return PromptResult::unchanged;
}

void TextPrompt::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 20, 5))
        return;
    const int interior = bounds.width - 2;
    WindowFrame::render(buffer, bounds, label_);
    write_padded(buffer, bounds.x + 1, bounds.y + 1, interior, "> " + value_, {.reverse = true});
    write_padded(buffer, bounds.x + 1, bounds.y + 2, interior, error_.empty() ? instructions_ : error_);
}

} // namespace vulpes::ui
