#include "vulpes/terminal/frame_diff.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <stdexcept>

namespace vulpes::terminal {
namespace {

void append_changed_cell(std::string& text, const Cell& cell) {
    if (!cell.continuation)
        text += encode_utf8(cell.glyph);
}

} // namespace

auto diff_frames(const ScreenBuffer& previous, const ScreenBuffer& current) -> std::vector<RenderOperation> {
    if (previous.width() != current.width() || previous.height() != current.height()) {
        throw std::invalid_argument{"screen buffers must have equal dimensions"};
    }

    std::vector<RenderOperation> operations;
    for (int y = 0; y < current.height(); ++y) {
        int x = 0;
        while (x < current.width()) {
            if (previous.cell(x, y) == current.cell(x, y)) {
                ++x;
                continue;
            }
            const int start = x;
            const Style style = current.cell(x, y).style;
            std::string text;
            while (x < current.width() && previous.cell(x, y) != current.cell(x, y) &&
                   current.cell(x, y).style == style) {
                append_changed_cell(text, current.cell(x, y));
                ++x;
            }
            operations.push_back({RenderOperationKind::move_cursor, start, y, {}, {}});
            operations.push_back({RenderOperationKind::set_style, 0, 0, style, {}});
            if (!text.empty())
                operations.push_back({RenderOperationKind::write, 0, 0, {}, std::move(text)});
        }
    }
    return operations;
}

} // namespace vulpes::terminal
