#include "vulpes/ui/multiline_editor.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <utility>

namespace vulpes::ui {
namespace {

struct LineSpan {
    std::size_t begin{};
    std::size_t end{};
};

struct GlyphSpan {
    std::size_t begin{};
    std::size_t end{};
    int cell{};
    int width{};
};

[[nodiscard]] auto is_continuation_byte(char byte) noexcept -> bool {
    return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

[[nodiscard]] auto lines(std::string_view text) -> std::vector<LineSpan> {
    std::vector<LineSpan> result;
    std::size_t begin{};
    while (begin <= text.size()) {
        const auto end = text.find('\n', begin);
        result.push_back({.begin = begin, .end = end == std::string_view::npos ? text.size() : end});
        if (end == std::string_view::npos)
            break;
        begin = end + 1;
    }
    return result;
}

[[nodiscard]] auto glyphs(std::string_view text, LineSpan line) -> std::vector<GlyphSpan> {
    std::vector<GlyphSpan> result;
    int cell{};
    for (auto begin = line.begin; begin < line.end;) {
        auto end = begin + 1;
        while (end < line.end && is_continuation_byte(text[end]))
            ++end;
        const auto width = std::max(0, terminal::text_width(text.substr(begin, end - begin)));
        result.push_back({.begin = begin, .end = end, .cell = cell, .width = width});
        cell += width;
        begin = end;
    }
    return result;
}

[[nodiscard]] auto line_index_at(const std::vector<LineSpan>& line_spans, std::size_t cursor) -> std::size_t {
    const auto found = std::ranges::upper_bound(line_spans, cursor, {}, &LineSpan::begin);
    return found == line_spans.begin() ? 0 : static_cast<std::size_t>(std::distance(line_spans.begin(), found) - 1);
}

[[nodiscard]] auto display_column(std::string_view text, LineSpan line, std::size_t cursor) -> int {
    return terminal::text_width(text.substr(line.begin, cursor - line.begin));
}

[[nodiscard]] auto offset_at_column(std::string_view text, LineSpan line, int target_column) -> std::size_t {
    auto offset = line.begin;
    for (const auto& glyph : glyphs(text, line)) {
        if (glyph.cell + glyph.width > target_column)
            break;
        offset = glyph.end;
    }
    return offset;
}

[[nodiscard]] auto clip_line(std::string_view text, LineSpan line, int first_cell, int width) -> MultilineViewportLine {
    MultilineViewportLine result;
    const auto units = glyphs(text, line);
    const auto total_width = units.empty() ? 0 : units.back().cell + units.back().width;
    result.clipped_left = first_cell > 0;
    result.clipped_right = total_width > first_cell + width;
    auto next_cell = first_cell;
    for (const auto& glyph : units) {
        if (glyph.cell < first_cell)
            continue;
        if (glyph.cell + glyph.width > first_cell + width)
            break;
        if (glyph.cell > next_cell)
            result.text.append(static_cast<std::size_t>(glyph.cell - next_cell), ' ');
        result.text.append(text.substr(glyph.begin, glyph.end - glyph.begin));
        next_cell = glyph.cell + glyph.width;
    }
    return result;
}

} // namespace

MultilineEditor::MultilineEditor(std::string text) : text_{std::move(text)}, cursor_{text_.size()} {
}

void MultilineEditor::set_text(std::string text) {
    text_ = std::move(text);
    cursor_ = text_.size();
    preferred_column_.reset();
    first_visible_line_ = 0;
    horizontal_offset_ = 0;
}

auto MultilineEditor::previous_offset() const noexcept -> std::size_t {
    if (cursor_ == 0)
        return 0;
    auto offset = cursor_ - 1;
    while (offset > 0 && is_continuation_byte(text_[offset]))
        --offset;
    return offset;
}

auto MultilineEditor::next_offset() const noexcept -> std::size_t {
    if (cursor_ >= text_.size())
        return text_.size();
    auto offset = cursor_ + 1;
    while (offset < text_.size() && is_continuation_byte(text_[offset]))
        ++offset;
    return offset;
}

auto MultilineEditor::move_vertical(int line_delta) -> MultilineEditResult {
    const auto line_spans = lines(text_);
    const auto current_line = line_index_at(line_spans, cursor_);
    if (!preferred_column_)
        preferred_column_ = display_column(text_, line_spans[current_line], cursor_);
    const auto target = std::clamp(static_cast<long long>(current_line) + line_delta, 0LL,
                                   static_cast<long long>(line_spans.size() - 1));
    const auto next = offset_at_column(text_, line_spans[static_cast<std::size_t>(target)], *preferred_column_);
    if (next == cursor_)
        return MultilineEditResult::unchanged;
    cursor_ = next;
    return MultilineEditResult::cursor_moved;
}

auto MultilineEditor::handle(const terminal::KeyEvent& event) -> MultilineEditResult {
    switch (event.key) {
    case terminal::Key::left: {
        reset_preferred_column();
        const auto previous = previous_offset();
        if (previous == cursor_)
            return MultilineEditResult::unchanged;
        cursor_ = previous;
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::right: {
        reset_preferred_column();
        const auto next = next_offset();
        if (next == cursor_)
            return MultilineEditResult::unchanged;
        cursor_ = next;
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::up:
        return move_vertical(-1);
    case terminal::Key::down:
        return move_vertical(1);
    case terminal::Key::page_up:
        return move_vertical(-std::max(1, last_view_height_ - 1));
    case terminal::Key::page_down:
        return move_vertical(std::max(1, last_view_height_ - 1));
    case terminal::Key::home: {
        reset_preferred_column();
        const auto line_spans = lines(text_);
        const auto target = line_spans[line_index_at(line_spans, cursor_)].begin;
        if (target == cursor_)
            return MultilineEditResult::unchanged;
        cursor_ = target;
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::end: {
        reset_preferred_column();
        const auto line_spans = lines(text_);
        const auto target = line_spans[line_index_at(line_spans, cursor_)].end;
        if (target == cursor_)
            return MultilineEditResult::unchanged;
        cursor_ = target;
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::enter:
        reset_preferred_column();
        text_.insert(cursor_, 1, '\n');
        ++cursor_;
        return MultilineEditResult::changed;
    case terminal::Key::tab: {
        reset_preferred_column();
        const auto line_spans = lines(text_);
        const auto column = display_column(text_, line_spans[line_index_at(line_spans, cursor_)], cursor_);
        const auto count = static_cast<std::size_t>(4 - column % 4);
        text_.insert(cursor_, count, ' ');
        cursor_ += count;
        return MultilineEditResult::changed;
    }
    case terminal::Key::backspace: {
        reset_preferred_column();
        const auto previous = previous_offset();
        if (previous == cursor_)
            return MultilineEditResult::unchanged;
        text_.erase(previous, cursor_ - previous);
        cursor_ = previous;
        return MultilineEditResult::changed;
    }
    case terminal::Key::delete_key: {
        reset_preferred_column();
        const auto next = next_offset();
        if (next == cursor_)
            return MultilineEditResult::unchanged;
        text_.erase(cursor_, next - cursor_);
        return MultilineEditResult::changed;
    }
    case terminal::Key::character: {
        reset_preferred_column();
        if (event.ctrl || event.alt || event.character == U'\0' || terminal::cell_width(event.character) < 0)
            return MultilineEditResult::unchanged;
        const auto encoded = terminal::encode_utf8(event.character);
        text_.insert(cursor_, encoded);
        cursor_ += encoded.size();
        return MultilineEditResult::changed;
    }
    default:
        return MultilineEditResult::unchanged;
    }
}

auto MultilineEditor::viewport(int width, int height) -> MultilineViewport {
    if (width <= 0 || height <= 0)
        return {};
    last_view_height_ = height;
    const auto line_spans = lines(text_);
    const auto cursor_line = line_index_at(line_spans, cursor_);
    const auto cursor_column = display_column(text_, line_spans[cursor_line], cursor_);

    const auto visible_height = static_cast<std::size_t>(height);
    if (cursor_line < first_visible_line_)
        first_visible_line_ = cursor_line;
    else if (cursor_line >= first_visible_line_ + visible_height)
        first_visible_line_ = cursor_line - visible_height + 1;
    const auto maximum_first = line_spans.size() > visible_height ? line_spans.size() - visible_height : 0;
    first_visible_line_ = std::min(first_visible_line_, maximum_first);

    if (cursor_column < horizontal_offset_)
        horizontal_offset_ = cursor_column;
    else if (cursor_column >= horizontal_offset_ + width)
        horizontal_offset_ = cursor_column - width + 1;

    MultilineViewport result{.first_line = first_visible_line_,
                             .cursor_row = static_cast<int>(cursor_line - first_visible_line_),
                             .cursor_column = cursor_column - horizontal_offset_,
                             .clipped_above = first_visible_line_ > 0,
                             .clipped_below = first_visible_line_ + visible_height < line_spans.size()};
    const auto line_end = std::min(line_spans.size(), first_visible_line_ + visible_height);
    result.lines.reserve(line_end - first_visible_line_);
    for (auto line = first_visible_line_; line < line_end; ++line)
        result.lines.push_back(clip_line(text_, line_spans[line], horizontal_offset_, width));
    return result;
}

} // namespace vulpes::ui
