#include "vulpes/terminal/cpp_terminal_adapter.hpp"

namespace vulpes::terminal {
namespace {

constexpr std::int32_t unicode_limit{0x10FFFF};
constexpr std::int32_t alt_modifier{1 << 22};
constexpr std::int32_t ctrl_modifier{1 << 23};

auto special_key(std::int32_t value) noexcept -> Key {
    switch (value) {
    case unicode_limit + 1:
        return Key::left;
    case unicode_limit + 2:
        return Key::right;
    case unicode_limit + 3:
        return Key::up;
    case unicode_limit + 4:
        return Key::down;
    case unicode_limit + 6:
        return Key::home;
    case unicode_limit + 7:
        return Key::insert_key;
    case unicode_limit + 8:
        return Key::end;
    case unicode_limit + 9:
        return Key::page_up;
    case unicode_limit + 10:
        return Key::page_down;
    case unicode_limit + 11:
        return Key::f1;
    case unicode_limit + 12:
        return Key::f2;
    case unicode_limit + 13:
        return Key::f3;
    case unicode_limit + 14:
        return Key::f4;
    case unicode_limit + 15:
        return Key::f5;
    case unicode_limit + 16:
        return Key::f6;
    case unicode_limit + 17:
        return Key::f7;
    case unicode_limit + 18:
        return Key::f8;
    case unicode_limit + 19:
        return Key::f9;
    case unicode_limit + 20:
        return Key::f10;
    case unicode_limit + 21:
        return Key::f11;
    case unicode_limit + 22:
        return Key::f12;
    default:
        return Key::unknown;
    }
}

} // namespace

auto normalize_cpp_terminal_key(std::int32_t encoded_key) noexcept -> KeyEvent {
    KeyEvent event;
    event.alt = (encoded_key & alt_modifier) != 0;
    event.ctrl = (encoded_key & ctrl_modifier) != 0;
    const auto value = encoded_key & ~(alt_modifier | ctrl_modifier);

    if (value >= 1 && value <= 26)
        event.ctrl = true;

    switch (value) {
    case 8:
        event.key = Key::backspace;
        return event;
    case 9:
        event.key = Key::tab;
        return event;
    case 13:
        event.key = Key::enter;
        return event;
    case 27:
        event.key = Key::escape;
        return event;
    case 127:
        event.key = Key::delete_key;
        return event;
    default:
        break;
    }

    event.key = special_key(value);
    if (event.key != Key::unknown)
        return event;

    if (value >= 0 && value <= unicode_limit) {
        event.key = Key::character;
        event.character = normalize_control_character(static_cast<char32_t>(value));
    }
    return event;
}

} // namespace vulpes::terminal
