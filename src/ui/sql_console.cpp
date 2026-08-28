#include "vulpes/ui/sql_console.hpp"

#include "vulpes/terminal/unicode.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <algorithm>
#include <cctype>

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

SqlConsole::SqlConsole(std::string title, std::string instructions, const Theme& theme, core::Clipboard* clipboard)
    : title_{std::move(title)}, instructions_{std::move(instructions)}, theme_{&theme}, clipboard_{clipboard} {
}

void SqlConsole::set_error(std::string message) {
    error_ = std::move(message);
    status_.clear();
}

void SqlConsole::set_status(std::string message) {
    status_ = std::move(message);
    error_.clear();
}

void SqlConsole::remember_script() {
    const auto script = editor_.text();
    if (std::ranges::all_of(script,
                            [](char character) { return std::isspace(static_cast<unsigned char>(character)) != 0; })) {
        return;
    }
    if (history_.empty() || history_.back() != script)
        history_.emplace_back(script);
    if (history_.size() > maximum_history_)
        history_.erase(history_.begin());
    history_index_.reset();
    history_draft_.clear();
}

auto SqlConsole::move_history(int direction) -> bool {
    if (history_.empty())
        return false;
    if (!history_index_) {
        if (direction > 0)
            return false;
        history_draft_ = editor_.text();
        history_index_ = history_.size();
    }
    if (direction < 0) {
        if (*history_index_ == 0)
            return false;
        --*history_index_;
        editor_.set_text(history_.at(*history_index_));
        return true;
    }
    if (*history_index_ + 1 < history_.size()) {
        ++*history_index_;
        editor_.set_text(history_.at(*history_index_));
        return true;
    }
    editor_.set_text(history_draft_);
    history_index_.reset();
    history_draft_.clear();
    return true;
}

auto SqlConsole::handle(const terminal::InputEvent& event) -> SqlConsoleResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (std::holds_alternative<terminal::ResizeEvent>(event))
        return SqlConsoleResult::redraw;
    if (key != nullptr && key->key == terminal::Key::escape)
        return SqlConsoleResult::cancelled;
    if (key != nullptr && key->key == terminal::Key::f8) {
        remember_script();
        return SqlConsoleResult::execute;
    }
    if (key != nullptr && key->ctrl && key->key == terminal::Key::up)
        return move_history(-1) ? SqlConsoleResult::redraw : SqlConsoleResult::unchanged;
    if (key != nullptr && key->ctrl && key->key == terminal::Key::down)
        return move_history(1) ? SqlConsoleResult::redraw : SqlConsoleResult::unchanged;
    const auto result = editor_.handle(event, clipboard_);
    if (result != MultilineEditResult::unchanged) {
        if (result == MultilineEditResult::changed) {
            error_.clear();
            history_index_.reset();
            history_draft_.clear();
        }
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
        if (static_cast<std::size_t>(row) < view.lines.size()) {
            const auto selection = view.lines[static_cast<std::size_t>(row)].selection_columns;
            if (selection) {
                for (int column = selection->first; column < selection->second; ++column) {
                    const auto x = bounds.x + 1 + gutter_width + column;
                    const auto cell = buffer.cell(x, y);
                    if (cell.continuation)
                        continue;
                    auto selected_style = cell.style;
                    selected_style.reverse = !selected_style.reverse;
                    buffer.put(x, y, cell.glyph, selected_style);
                }
            }
        }
    }

    const auto cursor_y = bounds.y + 1 + std::clamp(view.cursor_row, 0, line_count - 1);
    if (focused_) {
        const auto cursor_x = bounds.x + 1 + gutter_width + std::clamp(view.cursor_column, 0, editor_width - 1);
        auto cursor_style = buffer.cell(cursor_x, cursor_y).style;
        if (editor_.selection())
            cursor_style.underline = true;
        else
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
