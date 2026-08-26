#pragma once

#include "vulpes/terminal/terminal.hpp"

#include <deque>
#include <vector>

namespace vulpes::terminal {

class TestTerminal final : public Terminal {
public:
    explicit TestTerminal(Size size) : size_{size} {}

    [[nodiscard]] auto size() const -> Size override { return size_; }
    [[nodiscard]] auto read_event() -> InputEvent override;
    void present(const ScreenBuffer& previous, const ScreenBuffer& current) override;

    void enqueue(InputEvent event);
    void resize(Size size) { size_ = size; }
    [[nodiscard]] auto frames() const noexcept -> const std::vector<ScreenBuffer>& { return frames_; }

private:
    Size size_;
    std::deque<InputEvent> events_;
    std::vector<ScreenBuffer> frames_;
};

} // namespace vulpes::terminal

