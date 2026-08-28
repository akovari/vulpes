#include "vulpes/ui/line_editor.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <vector>

namespace vulpes::ui {
namespace {

struct CodePointSpan {
    std::size_t begin{};
    int width{};
    int cell{};
};

[[nodiscard]] auto is_continuation_byte(char byte) noexcept -> bool {
    return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

[[nodiscard]] auto spans(std::string_view text) -> std::vector<CodePointSpan> {
    std::vector<CodePointSpan> result;
    int cell{};
    for (std::size_t begin = 0; begin < text.size();) {
        auto end = begin + 1;
        while (end < text.size() && is_continuation_byte(text[end]))
            ++end;
        const auto width = std::max(0, terminal::text_width(text.substr(begin, end - begin)));
        result.push_back({.begin = begin, .width = width, .cell = cell});
        cell += width;
        begin = end;
    }
    return result;
}

} // namespace

LineEditor::LineEditor(std::string text) : text_{std::move(text)}, cursor_{text_.size()} {
}

void LineEditor::set_text(std::string text) {
    text_ = std::move(text);
    cursor_ = text_.size();
}

auto LineEditor::previous_offset() const noexcept -> std::size_t {
    if (cursor_ == 0)
        return 0;
    auto offset = cursor_ - 1;
    while (offset > 0 && is_continuation_byte(text_[offset]))
        --offset;
    return offset;
}

auto LineEditor::next_offset() const noexcept -> std::size_t {
    if (cursor_ >= text_.size())
        return text_.size();
    auto offset = cursor_ + 1;
    while (offset < text_.size() && is_continuation_byte(text_[offset]))
        ++offset;
    return offset;
}

auto LineEditor::handle(const terminal::KeyEvent& event) -> LineEditResult {
    switch (event.key) {
    case terminal::Key::left: {
        const auto previous = previous_offset();
        if (previous == cursor_)
            return LineEditResult::unchanged;
        cursor_ = previous;
        return LineEditResult::cursor_moved;
    }
    case terminal::Key::right: {
        const auto next = next_offset();
        if (next == cursor_)
            return LineEditResult::unchanged;
        cursor_ = next;
        return LineEditResult::cursor_moved;
    }
    case terminal::Key::home:
        if (cursor_ == 0)
            return LineEditResult::unchanged;
        cursor_ = 0;
        return LineEditResult::cursor_moved;
    case terminal::Key::end:
        if (cursor_ == text_.size())
            return LineEditResult::unchanged;
        cursor_ = text_.size();
        return LineEditResult::cursor_moved;
    case terminal::Key::backspace: {
        const auto previous = previous_offset();
        if (previous == cursor_)
            return LineEditResult::unchanged;
        text_.erase(previous, cursor_ - previous);
        cursor_ = previous;
        return LineEditResult::changed;
    }
    case terminal::Key::delete_key: {
        const auto next = next_offset();
        if (next == cursor_)
            return LineEditResult::unchanged;
        text_.erase(cursor_, next - cursor_);
        return LineEditResult::changed;
    }
    case terminal::Key::character: {
        if (event.ctrl || event.alt || event.character == U'\0' || terminal::cell_width(event.character) < 0)
            return LineEditResult::unchanged;
        const auto encoded = terminal::encode_utf8(event.character);
        text_.insert(cursor_, encoded);
        cursor_ += encoded.size();
        return LineEditResult::changed;
    }
    default:
        return LineEditResult::unchanged;
    }
}

auto LineEditor::viewport(int width, bool follow_cursor) const -> LineViewport {
    if (width <= 0)
        return {};
    const auto units = spans(text_);
    const auto cursor_unit = static_cast<std::size_t>(
        std::distance(units.begin(), std::ranges::find(units, cursor_, &CodePointSpan::begin)));
    const auto normalized_cursor = std::min(cursor_unit, units.size());
    const auto total_width = units.empty() ? 0 : units.back().cell + units.back().width;
    const auto cursor_cell = normalized_cursor < units.size() ? units[normalized_cursor].cell : total_width;
    std::size_t first{};
    if (follow_cursor) {
        while (first < normalized_cursor && cursor_cell - units[first].cell >= width)
            ++first;
    }
    std::size_t last = first;
    int used{};
    while (last < units.size() && used + units[last].width <= width) {
        used += units[last].width;
        ++last;
    }
    const auto begin_byte = first < units.size() ? units[first].begin : text_.size();
    const auto end_byte = last < units.size() ? units[last].begin : text_.size();
    const auto first_cell = first < units.size() ? units[first].cell : total_width;
    return {.text = text_.substr(begin_byte, end_byte - begin_byte),
            .cursor_column = follow_cursor ? cursor_cell - first_cell : 0,
            .clipped_left = first > 0,
            .clipped_right = last < units.size()};
}

void LineEditor::render(terminal::ScreenBuffer& buffer, Rect bounds, terminal::Style style, bool focused) const {
    if (bounds.width <= 0 || bounds.height <= 0 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height()) {
        return;
    }
    const auto view = viewport(bounds.width, focused);
    const auto end = buffer.write_utf8(bounds.x, bounds.y, view.text, style);
    for (int column = end; column < bounds.x + bounds.width; ++column)
        buffer.put(column, bounds.y, U' ', style);
    if (!focused)
        return;
    const auto cursor_column = std::clamp(view.cursor_column, 0, bounds.width - 1);
    auto cursor_style = buffer.cell(bounds.x + cursor_column, bounds.y).style;
    cursor_style.reverse = !cursor_style.reverse;
    const auto glyph = buffer.cell(bounds.x + cursor_column, bounds.y).glyph;
    buffer.put(bounds.x + cursor_column, bounds.y, glyph, cursor_style);
}

} // namespace vulpes::ui
