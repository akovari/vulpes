#pragma once

#include <string>

namespace vulpes::terminal {

enum class Key {
    character,
    enter,
    escape,
    tab,
    backspace,
    up,
    down,
    left,
    right,
    home,
    end,
    page_up,
    page_down,
    insert_key,
    delete_key,
    f1,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12,
    unknown
};

struct KeyEvent {
    Key key{Key::unknown};
    char32_t character{U'\0'};
    bool ctrl{false};
    bool alt{false};
    bool shift{false};
};

// Terminal hosts commonly report Ctrl+A through Ctrl+Z as ASCII control bytes.
// Normalize those bytes to their printable base character while retaining the
// modifier, so application actions do not depend on host-specific encodings.
[[nodiscard]] constexpr auto normalize_control_character(char32_t character) noexcept -> char32_t {
    return character >= U'\x01' && character <= U'\x1A' ? U'a' + (character - U'\x01') : character;
}

struct ResizeEvent {
    int width{};
    int height{};
};

// A complete UTF-8 paste payload. Backends normalize bracketed paste or their
// native equivalent into one event so editors never interpret pasted escape
// sequences as commands.
struct PasteEvent {
    std::string text;
};

} // namespace vulpes::terminal
