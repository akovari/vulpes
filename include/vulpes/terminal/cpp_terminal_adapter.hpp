#pragma once

#include "vulpes/terminal/input.hpp"

#include <cstdint>

namespace vulpes::terminal {

// CPP-Terminal represents modifiers in the high bits of its integer key
// values. Keeping this conversion free of CPP-Terminal types makes the
// dependency an implementation detail and allows deterministic unit tests.
[[nodiscard]] auto normalize_cpp_terminal_key(std::int32_t encoded_key) noexcept -> KeyEvent;

} // namespace vulpes::terminal
