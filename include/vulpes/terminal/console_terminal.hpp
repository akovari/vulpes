#pragma once

#include "vulpes/terminal/terminal.hpp"

#include <memory>

namespace vulpes::terminal {

// Owns the CPP-Terminal session and normalizes its platform-native events. The
// rest of Vulpes only depends on Terminal, never on CPP-Terminal directly.
class ConsoleTerminal final : public Terminal {
  public:
    ConsoleTerminal();
    ~ConsoleTerminal() override;

    ConsoleTerminal(const ConsoleTerminal&) = delete;
    auto operator=(const ConsoleTerminal&) -> ConsoleTerminal& = delete;
    ConsoleTerminal(ConsoleTerminal&&) noexcept;
    auto operator=(ConsoleTerminal&&) noexcept -> ConsoleTerminal&;

    [[nodiscard]] auto size() const -> Size override;
    [[nodiscard]] auto read_event() -> InputEvent override;
    void present(const ScreenBuffer& previous, const ScreenBuffer& current) override;

  private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

} // namespace vulpes::terminal
