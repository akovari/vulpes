#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace vulpes::ui {

// A small semantic focus model for keyboard-driven controls. It deliberately
// knows nothing about rendering or terminal keys; controls decide which keys
// move forward or backward, while disabled/read-only items are skipped here.
class FocusRing {
  public:
    FocusRing() = default;
    explicit FocusRing(std::vector<bool> focusable);

    void reset(std::vector<bool> focusable);
    [[nodiscard]] auto current() const noexcept -> std::optional<std::size_t>;
    [[nodiscard]] auto select(std::size_t index) -> bool;
    [[nodiscard]] auto move(int direction) -> bool;

  private:
    std::vector<bool> focusable_;
    std::optional<std::size_t> current_;
};

} // namespace vulpes::ui
