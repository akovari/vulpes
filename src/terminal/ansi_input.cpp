#include "vulpes/terminal/ansi_input.hpp"

namespace vulpes::terminal {

auto decode_ansi_key_sequence(std::string_view sequence) noexcept -> Key {
    if (sequence == "[A")
        return Key::up;
    if (sequence == "[B")
        return Key::down;
    if (sequence == "[C")
        return Key::right;
    if (sequence == "[D")
        return Key::left;
    if (sequence == "[H" || sequence == "[1~" || sequence == "[7~")
        return Key::home;
    if (sequence == "[F" || sequence == "[4~" || sequence == "[8~")
        return Key::end;
    if (sequence == "[5~")
        return Key::page_up;
    if (sequence == "[6~")
        return Key::page_down;
    if (sequence == "[2~")
        return Key::insert_key;
    if (sequence == "[3~")
        return Key::delete_key;

    if (sequence == "OP" || sequence == "[11~")
        return Key::f1;
    if (sequence == "OQ" || sequence == "[12~")
        return Key::f2;
    if (sequence == "OR" || sequence == "[13~")
        return Key::f3;
    if (sequence == "OS" || sequence == "[14~")
        return Key::f4;
    if (sequence == "[15~")
        return Key::f5;
    if (sequence == "[17~")
        return Key::f6;
    if (sequence == "[18~")
        return Key::f7;
    if (sequence == "[19~")
        return Key::f8;
    if (sequence == "[20~")
        return Key::f9;
    if (sequence == "[21~")
        return Key::f10;
    if (sequence == "[23~")
        return Key::f11;
    if (sequence == "[24~")
        return Key::f12;
    return Key::unknown;
}

} // namespace vulpes::terminal
