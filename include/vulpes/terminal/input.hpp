#pragma once

namespace vulpes::terminal {

enum class Key {
    character, enter, escape, tab, backspace, up, down, left, right, home, end,
    page_up, page_down, insert_key, delete_key, f1, f2, f3, f4, f5, f6, f7, f8,
    f9, f10, f11, f12, unknown
};

struct KeyEvent {
    Key key{Key::unknown};
    char32_t character{U'\0'};
    bool ctrl{false};
    bool alt{false};
    bool shift{false};
};

struct ResizeEvent { int width{}; int height{}; };

} // namespace vulpes::terminal

