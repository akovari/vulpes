#include "vulpes/ui/sql_console.hpp"

#include "vulpes/terminal/unicode.hpp"
#include "vulpes/ui/window_frame.hpp"

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

SqlConsole::SqlConsole(std::string title, std::string instructions, const Theme& theme)
    : title_{std::move(title)}, instructions_{std::move(instructions)}, theme_{&theme} {
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
    const auto result = editor_.handle(*key);
    if (result != MultilineEditResult::unchanged) {
        if (result == MultilineEditResult::changed)
            error_.clear();
        return SqlConsoleResult::redraw;
    }
    return SqlConsoleResult::unchanged;
}

void SqlConsole::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    if (bounds.width < 20 || bounds.height < 6 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height())
        return;

    const int interior = bounds.width - 2;
    WindowFrame::render(buffer, bounds, title_, window_frame_appearance(*theme_));

    constexpr int gutter_width = 5;
    const auto line_count = std::max(1, bounds.height - 4);
    const auto editor_width = interior - gutter_width;
    const auto view = editor_.viewport(editor_width, line_count);
    for (int row = 0; row < line_count; ++row) {
        const int y = bounds.y + 1 + row;
        const auto line_number = view.first_line + static_cast<std::size_t>(row);
        const auto active = focused_ && row == view.cursor_row;
        const auto gutter = line_number == 0 ? "sql> " : "...> ";
        write_padded(buffer, bounds.x + 1, y, gutter_width, gutter,
                     theme_->style(active ? ThemeRole::active_tab : ThemeRole::grid_footer));
        write_padded(buffer, bounds.x + 1 + gutter_width, y, editor_width, "", theme_->style(ThemeRole::input));
        if (static_cast<std::size_t>(row) < view.lines.size())
            static_cast<void>(buffer.write_utf8(bounds.x + 1 + gutter_width, y,
                                                view.lines[static_cast<std::size_t>(row)].text,
                                                theme_->style(ThemeRole::input)));
    }

    const auto cursor_y = bounds.y + 1 + std::clamp(view.cursor_row, 0, line_count - 1);
    if (focused_) {
        const auto cursor_x = bounds.x + 1 + gutter_width + std::clamp(view.cursor_column, 0, editor_width - 1);
        auto cursor_style = buffer.cell(cursor_x, cursor_y).style;
        cursor_style.reverse = !cursor_style.reverse;
        buffer.put(cursor_x, cursor_y, buffer.cell(cursor_x, cursor_y).glyph, cursor_style);
    }
    if (view.clipped_above)
        buffer.put(bounds.x + bounds.width - 3, bounds.y, U'▲', theme_->style(ThemeRole::border));
    if (view.clipped_below)
        buffer.put(bounds.x + bounds.width - 3, bounds.y + bounds.height - 1, U'▼', theme_->style(ThemeRole::border));
    if (!view.lines.empty()) {
        const auto& active_line = view.lines.at(static_cast<std::size_t>(view.cursor_row));
        if (active_line.clipped_left)
            buffer.put(bounds.x, cursor_y, U'◀', theme_->style(ThemeRole::border));
        if (active_line.clipped_right)
            buffer.put(bounds.x + bounds.width - 1, cursor_y, U'▶', theme_->style(ThemeRole::border));
    }

    const int message_y = bounds.y + bounds.height - 2;
    write_padded(buffer, bounds.x + 1, message_y, interior,
                 error_.empty() ? (status_.empty() ? instructions_ : status_) : error_,
                 theme_->style(error_.empty() ? ThemeRole::muted_text : ThemeRole::error));
}

} // namespace vulpes::ui
