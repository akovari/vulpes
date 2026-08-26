#include "vulpes/ui/sql_console.hpp"

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

void erase_last_code_point(std::string& text) {
    if (text.empty())
        return;
    std::size_t offset = text.size() - 1;
    while (offset > 0 && (static_cast<unsigned char>(text[offset]) & 0xC0U) == 0x80U)
        --offset;
    text.erase(offset);
}

} // namespace

SqlConsole::SqlConsole(std::string title, std::string instructions)
    : title_{std::move(title)}, instructions_{std::move(instructions)} {
}

void SqlConsole::set_error(std::string message) {
    error_ = std::move(message);
    status_.clear();
}

void SqlConsole::set_status(std::string message) {
    status_ = std::move(message);
    error_.clear();
}

auto SqlConsole::handle(const terminal::InputEvent& event) -> SqlConsoleResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key == nullptr)
        return SqlConsoleResult::redraw;
    if (key->key == terminal::Key::escape)
        return SqlConsoleResult::cancelled;
    if (key->key == terminal::Key::f8)
        return SqlConsoleResult::execute;
    if (key->key == terminal::Key::enter) {
        script_ += '\n';
        error_.clear();
        return SqlConsoleResult::redraw;
    }
    if (key->key == terminal::Key::backspace) {
        erase_last_code_point(script_);
        error_.clear();
        return SqlConsoleResult::redraw;
    }
    if (key->key == terminal::Key::character && !key->ctrl && !key->alt) {
        script_ += terminal::encode_utf8(key->character);
        error_.clear();
        return SqlConsoleResult::redraw;
    }
    return SqlConsoleResult::unchanged;
}

void SqlConsole::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (bounds.width < 20 || bounds.height < 6 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height())
        return;

    const int interior = bounds.width - 2;
    buffer.put(bounds.x, bounds.y, U'+');
    for (int column = 0; column < interior; ++column)
        buffer.put(bounds.x + 1 + column, bounds.y, U'-');
    buffer.put(bounds.x + bounds.width - 1, bounds.y, U'+');
    write_padded(buffer, bounds.x + 2, bounds.y, interior - 2, title_);

    const auto line_count = (std::max)(1, bounds.height - 4);
    std::size_t line_start{};
    for (int line = 0; line < line_count; ++line) {
        const int y = bounds.y + 1 + line;
        buffer.put(bounds.x, y, U'|');
        const auto line_end = script_.find('\n', line_start);
        const auto source =
            script_.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
        write_padded(buffer, bounds.x + 1, y, interior, (line == 0 ? "sql> " : "...> ") + source,
                     {.reverse = line == 0});
        buffer.put(bounds.x + bounds.width - 1, y, U'|');
        if (line_end == std::string::npos)
            line_start = script_.size();
        else
            line_start = line_end + 1;
    }

    const int message_y = bounds.y + bounds.height - 2;
    buffer.put(bounds.x, message_y, U'|');
    write_padded(buffer, bounds.x + 1, message_y, interior,
                 error_.empty() ? (status_.empty() ? instructions_ : status_) : error_);
    buffer.put(bounds.x + bounds.width - 1, message_y, U'|');

    const int bottom = bounds.y + bounds.height - 1;
    buffer.put(bounds.x, bottom, U'+');
    for (int column = 0; column < interior; ++column)
        buffer.put(bounds.x + 1 + column, bottom, U'-');
    buffer.put(bounds.x + bounds.width - 1, bottom, U'+');
}

} // namespace vulpes::ui
