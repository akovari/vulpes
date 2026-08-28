#pragma once

#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/theme.hpp"

#include <span>
#include <string>
#include <string_view>

namespace vulpes::ui {

struct ShortcutHint {
    std::string key;
    std::string label;
};

// Renders either a status message or a sequence of localized shortcut hints.
// It owns no application state and does not assign keyboard bindings.
class StatusBar {
  public:
    static void render(terminal::ScreenBuffer& buffer, Rect bounds, const Theme& theme, std::string_view message,
                       std::span<const ShortcutHint> shortcuts);
};

} // namespace vulpes::ui
