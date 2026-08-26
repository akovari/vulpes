#pragma once

#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"

#include <variant>

namespace vulpes::terminal {

struct Size { int width{}; int height{}; };
using InputEvent = std::variant<KeyEvent, ResizeEvent>;

class Terminal {
public:
    virtual ~Terminal() = default;
    [[nodiscard]] virtual auto size() const -> Size = 0;
    [[nodiscard]] virtual auto read_event() -> InputEvent = 0;
    virtual void present(const ScreenBuffer& previous, const ScreenBuffer& current) = 0;
};

} // namespace vulpes::terminal
