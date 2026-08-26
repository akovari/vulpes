#include "vulpes/terminal/console_terminal.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/ansi_encoder.hpp"
#include "vulpes/terminal/ansi_input.hpp"
#include "vulpes/terminal/frame_diff.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <utf8proc.h>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace vulpes::terminal {

class ConsoleTerminal::Implementation {
  public:
#ifdef _WIN32
    HANDLE input{GetStdHandle(STD_INPUT_HANDLE)};
    HANDLE output{GetStdHandle(STD_OUTPUT_HANDLE)};
    DWORD input_mode{};
    DWORD output_mode{};
#else
    termios original{};
#endif
};

namespace {

#ifdef _WIN32
[[nodiscard]] auto map_key(WORD code) -> Key {
    switch (code) {
    case VK_RETURN:
        return Key::enter;
    case VK_ESCAPE:
        return Key::escape;
    case VK_TAB:
        return Key::tab;
    case VK_BACK:
        return Key::backspace;
    case VK_UP:
        return Key::up;
    case VK_DOWN:
        return Key::down;
    case VK_LEFT:
        return Key::left;
    case VK_RIGHT:
        return Key::right;
    case VK_HOME:
        return Key::home;
    case VK_END:
        return Key::end;
    case VK_PRIOR:
        return Key::page_up;
    case VK_NEXT:
        return Key::page_down;
    case VK_INSERT:
        return Key::insert_key;
    case VK_DELETE:
        return Key::delete_key;
    case VK_F1:
        return Key::f1;
    case VK_F2:
        return Key::f2;
    case VK_F3:
        return Key::f3;
    case VK_F4:
        return Key::f4;
    case VK_F5:
        return Key::f5;
    case VK_F6:
        return Key::f6;
    case VK_F7:
        return Key::f7;
    case VK_F8:
        return Key::f8;
    case VK_F9:
        return Key::f9;
    case VK_F10:
        return Key::f10;
    case VK_F11:
        return Key::f11;
    case VK_F12:
        return Key::f12;
    default:
        return Key::unknown;
    }
}
#else
[[nodiscard]] auto read_utf8_character(unsigned char first) -> char32_t {
    if (first < 0x80U)
        return static_cast<char32_t>(first);

    int length = 0;
    if ((first & 0xE0U) == 0xC0U)
        length = 2;
    else if ((first & 0xF0U) == 0xE0U)
        length = 3;
    else if ((first & 0xF8U) == 0xF0U)
        length = 4;
    else
        return U'\uFFFD';

    std::array<utf8proc_uint8_t, 4> bytes{};
    bytes.front() = first;
    for (int index = 1; index < length; ++index) {
        char byte{};
        if (read(STDIN_FILENO, &byte, 1) != 1)
            throw Error{ErrorCategory::terminal, "unable to read UTF-8 terminal input"};
        bytes[static_cast<std::size_t>(index)] = static_cast<unsigned char>(byte);
    }

    utf8proc_int32_t code_point{};
    const auto consumed = utf8proc_iterate(bytes.data(), length, &code_point);
    return consumed == length ? static_cast<char32_t>(code_point) : U'\uFFFD';
}

[[nodiscard]] auto read_ansi_sequence() -> Key {
    std::string sequence;
    sequence.reserve(8);
    for (int index = 0; index < 8; ++index) {
        char byte{};
        if (read(STDIN_FILENO, &byte, 1) != 1)
            return Key::unknown;
        sequence += byte;
        if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '~')
            return decode_ansi_key_sequence(sequence);
    }
    return Key::unknown;
}
#endif

} // namespace

ConsoleTerminal::ConsoleTerminal() : implementation_{std::make_unique<Implementation>()} {
#ifdef _WIN32
    if (implementation_->input == INVALID_HANDLE_VALUE || implementation_->output == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(implementation_->input, &implementation_->input_mode) ||
        !GetConsoleMode(implementation_->output, &implementation_->output_mode)) {
        throw Error{ErrorCategory::terminal, "Vulpes requires an interactive Windows console"};
    }
    // Ctrl+C must arrive as a normalized key event so the application can
    // unwind its modal state and restore the screen. Processed input would
    // terminate the process before this RAII object gets that opportunity.
    // Use an explicit event-oriented input mode. Inheriting line, echo, or
    // virtual-terminal input from Windows Terminal can turn arrows/Escape into
    // a byte stream instead of KEY_EVENT_RECORD values for ReadConsoleInputW.
    const DWORD input_mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
    const DWORD output_mode = implementation_->output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(implementation_->input, input_mode) || !SetConsoleMode(implementation_->output, output_mode)) {
        throw Error{ErrorCategory::terminal, "unable to configure Windows console"};
    }
#else
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || tcgetattr(STDIN_FILENO, &implementation_->original) != 0) {
        throw Error{ErrorCategory::terminal, "Vulpes requires an interactive terminal"};
    }
    auto raw = implementation_->original;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
        throw Error{ErrorCategory::terminal, "unable to configure terminal"};
#endif
    std::cout << "\x1B[2J\x1B[H" << std::flush;
}

ConsoleTerminal::~ConsoleTerminal() {
    if (!implementation_)
        return;
#ifdef _WIN32
    SetConsoleMode(implementation_->input, implementation_->input_mode);
    SetConsoleMode(implementation_->output, implementation_->output_mode);
#else
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &implementation_->original);
#endif
    std::cout << ansi_reset() << "\x1B[2J\x1B[H\x1B[?25h" << std::flush;
}

ConsoleTerminal::ConsoleTerminal(ConsoleTerminal&&) noexcept = default;
auto ConsoleTerminal::operator=(ConsoleTerminal&&) noexcept -> ConsoleTerminal& = default;

auto ConsoleTerminal::size() const -> Size {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(implementation_->output, &info))
        throw Error{ErrorCategory::terminal, "unable to read console size"};
    return {info.srWindow.Right - info.srWindow.Left + 1, info.srWindow.Bottom - info.srWindow.Top + 1};
#else
    winsize window{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0 || window.ws_col == 0 || window.ws_row == 0) {
        throw Error{ErrorCategory::terminal, "unable to read terminal size"};
    }
    return {static_cast<int>(window.ws_col), static_cast<int>(window.ws_row)};
#endif
}

auto ConsoleTerminal::read_event() -> InputEvent {
#ifdef _WIN32
    INPUT_RECORD record{};
    DWORD read{};
    while (ReadConsoleInputW(implementation_->input, &record, 1, &read)) {
        if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            const auto& event = record.Event.WindowBufferSizeEvent.dwSize;
            return ResizeEvent{event.X, event.Y};
        }
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
            continue;
        const auto& event = record.Event.KeyEvent;
        const DWORD modifiers = event.dwControlKeyState;
        KeyEvent key;
        key.ctrl = (modifiers & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        key.alt = (modifiers & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        key.shift = (modifiers & SHIFT_PRESSED) != 0;
        key.character = event.uChar.UnicodeChar;
        key.key = map_key(event.wVirtualKeyCode);
        if (key.key == Key::unknown && key.character == U'\0' && event.wVirtualKeyCode >= 'A' &&
            event.wVirtualKeyCode <= 'Z') {
            key.character = static_cast<char32_t>(U'a' + (event.wVirtualKeyCode - 'A'));
        }
        if (key.key == Key::unknown && key.ctrl)
            key.character = normalize_control_character(key.character);
        if (key.key == Key::unknown)
            key.key = key.character != U'\0' ? Key::character : Key::unknown;
        return key;
    }
    throw Error{ErrorCategory::terminal, "unable to read Windows console input"};
#else
    char byte{};
    if (read(STDIN_FILENO, &byte, 1) != 1)
        throw Error{ErrorCategory::terminal, "unable to read terminal input"};
    if (byte == '\x1B') {
        pollfd descriptor{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
        if (poll(&descriptor, 1, 30) <= 0)
            return KeyEvent{.key = Key::escape};
        return KeyEvent{.key = read_ansi_sequence()};
    }
    if (byte == '\r' || byte == '\n')
        return KeyEvent{.key = Key::enter};
    if (byte == '\t')
        return KeyEvent{.key = Key::tab};
    if (byte == 127)
        return KeyEvent{.key = Key::backspace};
    const auto character = read_utf8_character(static_cast<unsigned char>(byte));
    const bool control = character < U' ';
    return KeyEvent{.key = Key::character,
                    .character = control ? normalize_control_character(character) : character,
                    .ctrl = control};
#endif
}

void ConsoleTerminal::present(const ScreenBuffer& previous, const ScreenBuffer& current) {
    std::cout << "\x1B[?25l" << encode_ansi(diff_frames(previous, current)) << std::flush;
}

} // namespace vulpes::terminal
