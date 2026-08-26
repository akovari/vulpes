#pragma once

#include "vulpes/terminal/screen_buffer.hpp"

#include <string>
#include <vector>

namespace vulpes::terminal {

enum class RenderOperationKind { move_cursor, set_style, write };

struct RenderOperation {
    RenderOperationKind kind;
    int x{};
    int y{};
    Style style{};
    std::string text;
    auto operator==(const RenderOperation&) const -> bool = default;
};

[[nodiscard]] auto diff_frames(const ScreenBuffer& previous, const ScreenBuffer& current)
    -> std::vector<RenderOperation>;

} // namespace vulpes::terminal
