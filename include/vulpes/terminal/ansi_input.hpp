#pragma once

#include "vulpes/terminal/input.hpp"

#include <string_view>

namespace vulpes::terminal {

// Decodes the bytes after an ESC character into the common terminal key model.
// Unknown sequences remain semantic unknown events instead of leaking escape
// syntax above the terminal boundary.
[[nodiscard]] auto decode_ansi_key_sequence(std::string_view sequence) noexcept -> Key;

} // namespace vulpes::terminal
