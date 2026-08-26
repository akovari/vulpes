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

TextPrompt::TextPrompt(std::string label, std::string initial_value)
    : label_{std::move(label)}, value_{std::move(initial_value)} {
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
    if (bounds.width < 20 || bounds.height < 5 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height())
        return;
    const int interior = bounds.width - 2;
    buffer.put(bounds.x, bounds.y, U'+');
    for (int column = 0; column < interior; ++column)
        buffer.put(bounds.x + 1 + column, bounds.y, U'-');
    buffer.put(bounds.x + bounds.width - 1, bounds.y, U'+');

    for (int row = 1; row < bounds.height - 1; ++row) {
        const int y = bounds.y + row;
        buffer.put(bounds.x, y, U'|');
        buffer.put(bounds.x + bounds.width - 1, y, U'|');
    }
    write_padded(buffer, bounds.x + 1, bounds.y + 1, interior, label_);
    write_padded(buffer, bounds.x + 1, bounds.y + 2, interior, "> " + value_, {.reverse = true});
    write_padded(buffer, bounds.x + 1, bounds.y + 3, interior, error_.empty() ? "Enter Apply   Esc Cancel" : error_);

    const int bottom = bounds.y + bounds.height - 1;
    buffer.put(bounds.x, bottom, U'+');
    for (int column = 0; column < interior; ++column)
        buffer.put(bounds.x + 1 + column, bottom, U'-');
    buffer.put(bounds.x + bounds.width - 1, bottom, U'+');
}

} // namespace vulpes::ui
