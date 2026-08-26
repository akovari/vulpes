#pragma once

#include "vulpes/terminal/frame_diff.hpp"

#include <string>
#include <vector>

namespace vulpes::terminal {

[[nodiscard]] auto encode_ansi(const std::vector<RenderOperation>& operations) -> std::string;
[[nodiscard]] auto ansi_reset() -> std::string;

} // namespace vulpes::terminal

