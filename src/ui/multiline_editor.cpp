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
    char32_t code_point{};
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
        result.push_back({.begin = begin,
                          .end = end,
                          .cell = cell,
                          .width = width,
                          .code_point = terminal::first_code_point(text.substr(begin, end - begin))});
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

[[nodiscard]] auto clip_line(std::string_view text, LineSpan line, int first_cell, int width,
                             std::optional<std::pair<std::size_t, std::size_t>> selection) -> MultilineViewportLine {
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
    if (selection) {
        const auto begin = std::max(selection->first, line.begin);
        const auto end = std::min(selection->second, line.end);
        if (begin < end) {
            const auto selection_begin = display_column(text, line, begin) - first_cell;
            const auto selection_end = display_column(text, line, end) - first_cell;
            const auto visible_begin = std::clamp(selection_begin, 0, width);
            const auto visible_end = std::clamp(selection_end, 0, width);
            if (visible_begin < visible_end)
                result.selection_columns = std::pair{visible_begin, visible_end};
        }
    }
    return result;
}

[[nodiscard]] auto normalized_multiline_paste(std::string_view text) -> std::string {
    std::string result;
    const auto all = LineSpan{.begin = 0, .end = text.size()};
    for (const auto& glyph : glyphs(text, all)) {
        const auto code_point = glyph.code_point;
        if (code_point == U'\r') {
            if (glyph.end < text.size() && text[glyph.end] == '\n')
                continue;
            result.push_back('\n');
        } else if (code_point == U'\t') {
            result.append(4, ' ');
        } else if (code_point == U'\n' || (code_point >= U' ' && code_point != U'\x7F') || code_point >= U'\xA0') {
            result.append(text.substr(glyph.begin, glyph.end - glyph.begin));
        }
    }
    return result;
}

} // namespace

MultilineEditor::MultilineEditor(std::string text, TextEditorOptions options)
    : text_{std::move(text)}, cursor_{text_.size()}, mode_{options.initial_mode},
      allow_mode_toggle_{options.allow_mode_toggle} {
}

void MultilineEditor::set_text(std::string text) {
    text_ = std::move(text);
    cursor_ = text_.size();
    selection_anchor_.reset();
    preferred_column_.reset();
    first_visible_line_ = 0;
    horizontal_offset_ = 0;
    undo_.clear();
    redo_.clear();
    undo_bytes_ = 0;
}

auto MultilineEditor::selection() const noexcept -> std::optional<std::pair<std::size_t, std::size_t>> {
    if (!selection_anchor_ || *selection_anchor_ == cursor_)
        return std::nullopt;
    return std::minmax(*selection_anchor_, cursor_);
}

auto MultilineEditor::selected_text() const -> std::string {
    const auto selected = selection();
    return selected ? text_.substr(selected->first, selected->second - selected->first) : std::string{};
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

auto MultilineEditor::previous_word_offset() const noexcept -> std::size_t {
    const auto units = glyphs(text_, {.begin = 0, .end = text_.size()});
    auto found = std::ranges::lower_bound(units, cursor_, {}, &GlyphSpan::begin);
    auto index = static_cast<std::size_t>(std::distance(units.begin(), found));
    while (index > 0 && !terminal::is_word_code_point(units[index - 1].code_point))
        --index;
    while (index > 0 && terminal::is_word_code_point(units[index - 1].code_point))
        --index;
    return index < units.size() ? units[index].begin : 0;
}

auto MultilineEditor::next_word_offset() const noexcept -> std::size_t {
    const auto units = glyphs(text_, {.begin = 0, .end = text_.size()});
    auto found = std::ranges::lower_bound(units, cursor_, {}, &GlyphSpan::begin);
    auto index = static_cast<std::size_t>(std::distance(units.begin(), found));
    while (index < units.size() && terminal::is_word_code_point(units[index].code_point))
        ++index;
    while (index < units.size() && !terminal::is_word_code_point(units[index].code_point))
        ++index;
    return index < units.size() ? units[index].begin : text_.size();
}

void MultilineEditor::move_to(std::size_t offset, bool selecting) noexcept {
    if (selecting) {
        if (!selection_anchor_)
            selection_anchor_ = cursor_;
    } else {
        selection_anchor_.reset();
    }
    cursor_ = std::min(offset, text_.size());
    if (selection_anchor_ == cursor_)
        selection_anchor_.reset();
}

auto MultilineEditor::erase_selection() -> bool {
    const auto selected = selection();
    if (!selected)
        return false;
    text_.erase(selected->first, selected->second - selected->first);
    cursor_ = selected->first;
    selection_anchor_.reset();
    return true;
}

void MultilineEditor::save_undo() {
    redo_.clear();
    undo_bytes_ += text_.size();
    undo_.push_back({.text = text_, .cursor = cursor_, .selection_anchor = selection_anchor_});
    while (undo_.size() > maximum_undo_states_ || undo_bytes_ > maximum_undo_bytes_) {
        undo_bytes_ -= undo_.front().text.size();
        undo_.erase(undo_.begin());
    }
}

auto MultilineEditor::undo() -> bool {
    if (undo_.empty())
        return false;
    redo_.push_back({.text = text_, .cursor = cursor_, .selection_anchor = selection_anchor_});
    auto snapshot = std::move(undo_.back());
    undo_bytes_ -= snapshot.text.size();
    undo_.pop_back();
    text_ = std::move(snapshot.text);
    cursor_ = snapshot.cursor;
    selection_anchor_ = snapshot.selection_anchor;
    reset_preferred_column();
    return true;
}

auto MultilineEditor::redo() -> bool {
    if (redo_.empty())
        return false;
    undo_bytes_ += text_.size();
    undo_.push_back({.text = text_, .cursor = cursor_, .selection_anchor = selection_anchor_});
    while (undo_.size() > maximum_undo_states_ || undo_bytes_ > maximum_undo_bytes_) {
        undo_bytes_ -= undo_.front().text.size();
        undo_.erase(undo_.begin());
    }
    auto snapshot = std::move(redo_.back());
    redo_.pop_back();
    text_ = std::move(snapshot.text);
    cursor_ = snapshot.cursor;
    selection_anchor_ = snapshot.selection_anchor;
    reset_preferred_column();
    return true;
}

auto MultilineEditor::insert(std::string_view source) -> bool {
    auto value = normalized_multiline_paste(source);
    if (value.empty())
        return false;
    save_undo();
    const bool replaced_selection = erase_selection();
    if (replaced_selection || mode_ == TextEditMode::insert) {
        text_.insert(cursor_, value);
        cursor_ += value.size();
        return true;
    }
    for (const auto& glyph : glyphs(value, {.begin = 0, .end = value.size()})) {
        if (glyph.code_point != U'\n' && cursor_ < text_.size() && text_[cursor_] != '\n') {
            const auto next = next_offset();
            text_.erase(cursor_, next - cursor_);
        }
        const auto encoded = value.substr(glyph.begin, glyph.end - glyph.begin);
        text_.insert(cursor_, encoded);
        cursor_ += encoded.size();
    }
    return true;
}

auto MultilineEditor::move_vertical(int line_delta, bool selecting) -> MultilineEditResult {
    const auto line_spans = lines(text_);
    const auto current_line = line_index_at(line_spans, cursor_);
    if (!preferred_column_)
        preferred_column_ = display_column(text_, line_spans[current_line], cursor_);
    const auto target = std::clamp(static_cast<long long>(current_line) + line_delta, 0LL,
                                   static_cast<long long>(line_spans.size() - 1));
    const auto next = offset_at_column(text_, line_spans[static_cast<std::size_t>(target)], *preferred_column_);
    if (next == cursor_)
        return MultilineEditResult::unchanged;
    move_to(next, selecting);
    return MultilineEditResult::cursor_moved;
}

auto MultilineEditor::handle(const terminal::KeyEvent& event, core::Clipboard* clipboard) -> MultilineEditResult {
    const auto selected = selection();
    const auto character = terminal::lowercase_code_point(event.character);
    const bool copy = (event.key == terminal::Key::insert_key && event.ctrl) ||
                      (event.key == terminal::Key::character && event.ctrl && character == U'c');
    const bool cut = (event.key == terminal::Key::delete_key && event.shift) ||
                     (event.key == terminal::Key::character && event.ctrl && character == U'x');
    const bool paste = (event.key == terminal::Key::insert_key && event.shift) ||
                       (event.key == terminal::Key::character && event.ctrl && character == U'v');
    if (event.key == terminal::Key::character && event.ctrl && character == U'a') {
        if (text_.empty() || (selected && selected->first == 0 && selected->second == text_.size()))
            return MultilineEditResult::unchanged;
        selection_anchor_ = 0;
        cursor_ = text_.size();
        reset_preferred_column();
        return MultilineEditResult::cursor_moved;
    }
    if (event.key == terminal::Key::character && event.ctrl && character == U'z')
        return (event.shift ? redo() : undo()) ? MultilineEditResult::changed : MultilineEditResult::unchanged;
    if (event.key == terminal::Key::character && event.ctrl && character == U'y')
        return redo() ? MultilineEditResult::changed : MultilineEditResult::unchanged;
    if (copy) {
        if (clipboard != nullptr && selected)
            static_cast<void>(clipboard->write_text(selected_text()));
        return MultilineEditResult::unchanged;
    }
    if (cut) {
        if (!selected)
            return MultilineEditResult::unchanged;
        if (clipboard != nullptr)
            static_cast<void>(clipboard->write_text(selected_text()));
        save_undo();
        static_cast<void>(erase_selection());
        reset_preferred_column();
        return MultilineEditResult::changed;
    }
    if (paste) {
        if (clipboard == nullptr)
            return MultilineEditResult::unchanged;
        const auto text = clipboard->read_text();
        reset_preferred_column();
        return text && insert(*text) ? MultilineEditResult::changed : MultilineEditResult::unchanged;
    }
    switch (event.key) {
    case terminal::Key::left: {
        reset_preferred_column();
        if (!event.shift && selected) {
            move_to(selected->first, false);
            return MultilineEditResult::cursor_moved;
        }
        const auto previous = event.ctrl ? previous_word_offset() : previous_offset();
        if (previous == cursor_)
            return MultilineEditResult::unchanged;
        move_to(previous, event.shift);
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::right: {
        reset_preferred_column();
        if (!event.shift && selected) {
            move_to(selected->second, false);
            return MultilineEditResult::cursor_moved;
        }
        const auto next = event.ctrl ? next_word_offset() : next_offset();
        if (next == cursor_)
            return MultilineEditResult::unchanged;
        move_to(next, event.shift);
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::up:
        return move_vertical(-1, event.shift);
    case terminal::Key::down:
        return move_vertical(1, event.shift);
    case terminal::Key::page_up:
        return move_vertical(-std::max(1, last_view_height_ - 1), event.shift);
    case terminal::Key::page_down:
        return move_vertical(std::max(1, last_view_height_ - 1), event.shift);
    case terminal::Key::home: {
        reset_preferred_column();
        const auto line_spans = lines(text_);
        const auto target = event.ctrl ? std::size_t{0} : line_spans[line_index_at(line_spans, cursor_)].begin;
        if (target == cursor_ && (!selected || event.shift))
            return MultilineEditResult::unchanged;
        move_to(target, event.shift);
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::end: {
        reset_preferred_column();
        const auto line_spans = lines(text_);
        const auto target = event.ctrl ? text_.size() : line_spans[line_index_at(line_spans, cursor_)].end;
        if (target == cursor_ && (!selected || event.shift))
            return MultilineEditResult::unchanged;
        move_to(target, event.shift);
        return MultilineEditResult::cursor_moved;
    }
    case terminal::Key::enter:
        reset_preferred_column();
        return insert("\n") ? MultilineEditResult::changed : MultilineEditResult::unchanged;
    case terminal::Key::tab: {
        reset_preferred_column();
        const auto line_spans = lines(text_);
        const auto column = display_column(text_, line_spans[line_index_at(line_spans, cursor_)], cursor_);
        const auto count = static_cast<std::size_t>(4 - column % 4);
        const std::string indentation(count, ' ');
        return insert(indentation) ? MultilineEditResult::changed : MultilineEditResult::unchanged;
    }
    case terminal::Key::backspace: {
        reset_preferred_column();
        if (selected) {
            save_undo();
            static_cast<void>(erase_selection());
            return MultilineEditResult::changed;
        }
        const auto previous = previous_offset();
        if (previous == cursor_)
            return MultilineEditResult::unchanged;
        save_undo();
        text_.erase(previous, cursor_ - previous);
        cursor_ = previous;
        return MultilineEditResult::changed;
    }
    case terminal::Key::delete_key: {
        reset_preferred_column();
        if (selected) {
            save_undo();
            static_cast<void>(erase_selection());
            return MultilineEditResult::changed;
        }
        const auto next = next_offset();
        if (next == cursor_)
            return MultilineEditResult::unchanged;
        save_undo();
        text_.erase(cursor_, next - cursor_);
        return MultilineEditResult::changed;
    }
    case terminal::Key::character: {
        reset_preferred_column();
        if (event.ctrl || event.alt || event.character == U'\0' || terminal::cell_width(event.character) < 0)
            return MultilineEditResult::unchanged;
        const auto encoded = terminal::encode_utf8(event.character);
        return insert(encoded) ? MultilineEditResult::changed : MultilineEditResult::unchanged;
    }
    case terminal::Key::insert_key:
        if (!allow_mode_toggle_ || event.ctrl || event.alt || event.shift)
            return MultilineEditResult::unchanged;
        mode_ = mode_ == TextEditMode::insert ? TextEditMode::overwrite : TextEditMode::insert;
        return MultilineEditResult::cursor_moved;
    default:
        return MultilineEditResult::unchanged;
    }
}

auto MultilineEditor::handle(const terminal::InputEvent& event, core::Clipboard* clipboard) -> MultilineEditResult {
    if (const auto* key = std::get_if<terminal::KeyEvent>(&event))
        return handle(*key, clipboard);
    if (const auto* paste = std::get_if<terminal::PasteEvent>(&event)) {
        reset_preferred_column();
        return insert(paste->text) ? MultilineEditResult::changed : MultilineEditResult::unchanged;
    }
    return MultilineEditResult::unchanged;
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
        result.lines.push_back(clip_line(text_, line_spans[line], horizontal_offset_, width, selection()));
    return result;
}

} // namespace vulpes::ui
