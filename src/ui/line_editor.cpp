#include "vulpes/ui/line_editor.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <vector>

namespace vulpes::ui {
namespace {

struct CodePointSpan {
    std::size_t begin{};
    std::size_t end{};
    int width{};
    int cell{};
    char32_t code_point{};
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
        result.push_back({.begin = begin,
                          .end = end,
                          .width = width,
                          .cell = cell,
                          .code_point = terminal::first_code_point(text.substr(begin, end - begin))});
        cell += width;
        begin = end;
    }
    return result;
}

[[nodiscard]] auto normalized_line_paste(std::string_view text) -> std::string {
    std::string result;
    for (const auto& unit : spans(text)) {
        const auto code_point = unit.code_point;
        if (code_point == U'\r') {
            if (unit.end < text.size() && text[unit.end] == '\n')
                continue;
            result.push_back(' ');
        } else if (code_point == U'\n' || code_point == U'\t') {
            result.push_back(' ');
        } else if ((code_point >= U' ' && code_point != U'\x7F') || code_point >= U'\xA0') {
            result.append(text.substr(unit.begin, unit.end - unit.begin));
        }
    }
    return result;
}

} // namespace

LineEditor::LineEditor(std::string text, TextEditorOptions options)
    : text_{std::move(text)}, cursor_{text_.size()}, mode_{options.initial_mode},
      allow_mode_toggle_{options.allow_mode_toggle} {
}

void LineEditor::set_text(std::string text) {
    text_ = std::move(text);
    cursor_ = text_.size();
    selection_anchor_.reset();
}

auto LineEditor::selection() const noexcept -> std::optional<std::pair<std::size_t, std::size_t>> {
    if (!selection_anchor_ || *selection_anchor_ == cursor_)
        return std::nullopt;
    return std::minmax(*selection_anchor_, cursor_);
}

auto LineEditor::selected_text() const -> std::string {
    const auto selected = selection();
    return selected ? text_.substr(selected->first, selected->second - selected->first) : std::string{};
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

auto LineEditor::previous_word_offset() const noexcept -> std::size_t {
    const auto units = spans(text_);
    auto found = std::ranges::lower_bound(units, cursor_, {}, &CodePointSpan::begin);
    auto index = static_cast<std::size_t>(std::distance(units.begin(), found));
    while (index > 0 && !terminal::is_word_code_point(units[index - 1].code_point))
        --index;
    while (index > 0 && terminal::is_word_code_point(units[index - 1].code_point))
        --index;
    return index < units.size() ? units[index].begin : 0;
}

auto LineEditor::next_word_offset() const noexcept -> std::size_t {
    const auto units = spans(text_);
    auto found = std::ranges::lower_bound(units, cursor_, {}, &CodePointSpan::begin);
    auto index = static_cast<std::size_t>(std::distance(units.begin(), found));
    while (index < units.size() && terminal::is_word_code_point(units[index].code_point))
        ++index;
    while (index < units.size() && !terminal::is_word_code_point(units[index].code_point))
        ++index;
    return index < units.size() ? units[index].begin : text_.size();
}

void LineEditor::move_to(std::size_t offset, bool selecting) noexcept {
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

auto LineEditor::erase_selection() -> bool {
    const auto selected = selection();
    if (!selected)
        return false;
    text_.erase(selected->first, selected->second - selected->first);
    cursor_ = selected->first;
    selection_anchor_.reset();
    return true;
}

auto LineEditor::insert(std::string_view source) -> bool {
    auto value = normalized_line_paste(source);
    if (value.empty())
        return false;
    const bool replaced_selection = erase_selection();
    if (!replaced_selection && mode_ == TextEditMode::overwrite) {
        auto erase_end = cursor_;
        for (std::size_t count = spans(value).size(); count > 0 && erase_end < text_.size(); --count) {
            ++erase_end;
            while (erase_end < text_.size() && is_continuation_byte(text_[erase_end]))
                ++erase_end;
        }
        text_.erase(cursor_, erase_end - cursor_);
    }
    text_.insert(cursor_, value);
    cursor_ += value.size();
    return true;
}

auto LineEditor::handle(const terminal::KeyEvent& event, core::Clipboard* clipboard) -> LineEditResult {
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
            return LineEditResult::unchanged;
        selection_anchor_ = 0;
        cursor_ = text_.size();
        return LineEditResult::cursor_moved;
    }
    if (copy) {
        if (clipboard != nullptr && selected)
            static_cast<void>(clipboard->write_text(selected_text()));
        return LineEditResult::unchanged;
    }
    if (cut) {
        if (!selected)
            return LineEditResult::unchanged;
        if (clipboard != nullptr)
            static_cast<void>(clipboard->write_text(selected_text()));
        static_cast<void>(erase_selection());
        return LineEditResult::changed;
    }
    if (paste) {
        if (clipboard == nullptr)
            return LineEditResult::unchanged;
        const auto text = clipboard->read_text();
        return text && insert(*text) ? LineEditResult::changed : LineEditResult::unchanged;
    }
    switch (event.key) {
    case terminal::Key::left: {
        if (!event.shift && selected) {
            move_to(selected->first, false);
            return LineEditResult::cursor_moved;
        }
        const auto previous = event.ctrl ? previous_word_offset() : previous_offset();
        if (previous == cursor_)
            return LineEditResult::unchanged;
        move_to(previous, event.shift);
        return LineEditResult::cursor_moved;
    }
    case terminal::Key::right: {
        if (!event.shift && selected) {
            move_to(selected->second, false);
            return LineEditResult::cursor_moved;
        }
        const auto next = event.ctrl ? next_word_offset() : next_offset();
        if (next == cursor_)
            return LineEditResult::unchanged;
        move_to(next, event.shift);
        return LineEditResult::cursor_moved;
    }
    case terminal::Key::home:
        if (cursor_ == 0 && (!selected || event.shift))
            return LineEditResult::unchanged;
        move_to(0, event.shift);
        return LineEditResult::cursor_moved;
    case terminal::Key::end:
        if (cursor_ == text_.size() && (!selected || event.shift))
            return LineEditResult::unchanged;
        move_to(text_.size(), event.shift);
        return LineEditResult::cursor_moved;
    case terminal::Key::backspace: {
        if (erase_selection())
            return LineEditResult::changed;
        const auto previous = previous_offset();
        if (previous == cursor_)
            return LineEditResult::unchanged;
        text_.erase(previous, cursor_ - previous);
        cursor_ = previous;
        return LineEditResult::changed;
    }
    case terminal::Key::delete_key: {
        if (erase_selection())
            return LineEditResult::changed;
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
        return insert(encoded) ? LineEditResult::changed : LineEditResult::unchanged;
    }
    case terminal::Key::insert_key:
        if (!allow_mode_toggle_ || event.ctrl || event.alt || event.shift)
            return LineEditResult::unchanged;
        mode_ = mode_ == TextEditMode::insert ? TextEditMode::overwrite : TextEditMode::insert;
        return LineEditResult::cursor_moved;
    default:
        return LineEditResult::unchanged;
    }
}

auto LineEditor::handle(const terminal::InputEvent& event, core::Clipboard* clipboard) -> LineEditResult {
    if (const auto* key = std::get_if<terminal::KeyEvent>(&event))
        return handle(*key, clipboard);
    if (const auto* paste = std::get_if<terminal::PasteEvent>(&event))
        return insert(paste->text) ? LineEditResult::changed : LineEditResult::unchanged;
    return LineEditResult::unchanged;
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
    auto selected = selection();
    if (selected) {
        selected->first = std::max(selected->first, begin_byte);
        selected->second = std::min(selected->second, end_byte);
        if (selected->first >= selected->second)
            selected.reset();
    }
    return {.text = text_.substr(begin_byte, end_byte - begin_byte),
            .cursor_column = follow_cursor ? cursor_cell - first_cell : 0,
            .clipped_left = first > 0,
            .clipped_right = last < units.size(),
            .first_offset = begin_byte,
            .selection = selected};
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
    if (view.selection) {
        auto selected_style = style;
        selected_style.reverse = !selected_style.reverse;
        for (const auto& unit : spans(view.text)) {
            const auto absolute = view.first_offset + unit.begin;
            if (absolute >= view.selection->first && absolute < view.selection->second)
                buffer.put(bounds.x + unit.cell, bounds.y, unit.code_point, selected_style);
        }
    }
    if (!focused)
        return;
    const auto cursor_column = std::clamp(view.cursor_column, 0, bounds.width - 1);
    auto cursor_style = buffer.cell(bounds.x + cursor_column, bounds.y).style;
    if (view.selection)
        cursor_style.underline = true;
    else
        cursor_style.reverse = !cursor_style.reverse;
    const auto glyph = buffer.cell(bounds.x + cursor_column, bounds.y).glyph;
    buffer.put(bounds.x + cursor_column, bounds.y, glyph, cursor_style);
}

} // namespace vulpes::ui
