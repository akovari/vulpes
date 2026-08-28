#include "vulpes/ui/terminal_diagnostics.hpp"

#include "vulpes/terminal/unicode.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <string_view>
#include <utility>

namespace vulpes::ui {
namespace {

auto key_name(terminal::Key key) -> std::string_view {
    switch (key) {
    case terminal::Key::character:
        return "Character";
    case terminal::Key::enter:
        return "Enter";
    case terminal::Key::escape:
        return "Escape";
    case terminal::Key::tab:
        return "Tab";
    case terminal::Key::backspace:
        return "Backspace";
    case terminal::Key::up:
        return "Up";
    case terminal::Key::down:
        return "Down";
    case terminal::Key::left:
        return "Left";
    case terminal::Key::right:
        return "Right";
    case terminal::Key::home:
        return "Home";
    case terminal::Key::end:
        return "End";
    case terminal::Key::page_up:
        return "PageUp";
    case terminal::Key::page_down:
        return "PageDown";
    case terminal::Key::insert_key:
        return "Insert";
    case terminal::Key::delete_key:
        return "Delete";
    case terminal::Key::f1:
        return "F1";
    case terminal::Key::f2:
        return "F2";
    case terminal::Key::f3:
        return "F3";
    case terminal::Key::f4:
        return "F4";
    case terminal::Key::f5:
        return "F5";
    case terminal::Key::f6:
        return "F6";
    case terminal::Key::f7:
        return "F7";
    case terminal::Key::f8:
        return "F8";
    case terminal::Key::f9:
        return "F9";
    case terminal::Key::f10:
        return "F10";
    case terminal::Key::f11:
        return "F11";
    case terminal::Key::f12:
        return "F12";
    case terminal::Key::unknown:
        return "Unknown";
    }
    return "Unknown";
}

auto describe_key(const terminal::KeyEvent& event) -> std::string {
    std::string result{"Key: "};
    if (event.ctrl)
        result += "Ctrl+";
    if (event.alt)
        result += "Alt+";
    if (event.shift)
        result += "Shift+";
    result += key_name(event.key);
    if (event.key == terminal::Key::character) {
        result += " '";
        result += terminal::encode_utf8(event.character);
        result += "'";
    }
    return result;
}

void write(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text, terminal::Style style = {}) {
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

TerminalDiagnostics::TerminalDiagnostics(const core::Localizer& messages)
    : title_{messages.translate("terminal.diagnostics.title")},
      instructions_{messages.translate("terminal.diagnostics.instructions")},
      waiting_{messages.translate("terminal.diagnostics.waiting")} {
}

auto TerminalDiagnostics::handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult {
    if (action == core::ActionId::application_back || action == core::ActionId::application_quit)
        return DocumentResult::close;

    if (const auto* key = std::get_if<terminal::KeyEvent>(&event)) {
        append(describe_key(*key));
    } else if (const auto* resize = std::get_if<terminal::ResizeEvent>(&event)) {
        append("Resize: " + std::to_string(resize->width) + " x " + std::to_string(resize->height));
    } else if (const auto* paste = std::get_if<terminal::PasteEvent>(&event)) {
        append("Paste: " + std::to_string(paste->text.size()) + " UTF-8 bytes");
    }
    return DocumentResult::redraw;
}

void TerminalDiagnostics::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    if (!WindowFrame::fits(buffer, bounds, 40, 10))
        return;
    WindowFrame::render(buffer, bounds, title_);
    const auto content = WindowFrame::content_bounds(bounds);
    const int event_rows = content.height - 2;
    if (events_.empty())
        write(buffer, content.x, content.y, content.width, waiting_);
    else {
        const auto first = events_.size() > static_cast<std::size_t>(event_rows)
                               ? events_.size() - static_cast<std::size_t>(event_rows)
                               : 0;
        for (std::size_t index = first; index < events_.size(); ++index)
            write(buffer, content.x, content.y + static_cast<int>(index - first), content.width, events_[index]);
    }
    write(buffer, content.x, content.y + content.height - 1, content.width, instructions_, {.bold = true});
}

void TerminalDiagnostics::append(std::string event) {
    constexpr std::size_t maximum_events{100};
    events_.push_back(std::move(event));
    if (events_.size() > maximum_events)
        events_.erase(events_.begin());
}

} // namespace vulpes::ui
