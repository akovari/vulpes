#pragma once

#include "vulpes/terminal/screen_buffer.hpp"

#include <string_view>

namespace vulpes::ui {

// Roles describe intent rather than a screen's RGB values. Terminal renderers
// remain free to reduce these styles to their own capability set.
enum class ThemeRole {
    text,
    title,
    menu,
    menu_mnemonic,
    selection,
    popup,
    popup_selection,
    active_menu,
    active_menu_mnemonic,
    tab,
    active_tab,
    status_bar,
    status_bar_shortcut,
};

enum class ThemeName { midnight, high_contrast };

struct Theme {
    terminal::Style text;
    terminal::Style title;
    terminal::Style menu;
    terminal::Style menu_mnemonic;
    terminal::Style selection;
    terminal::Style popup;
    terminal::Style popup_selection;
    terminal::Style active_menu;
    terminal::Style active_menu_mnemonic;
    terminal::Style tab;
    terminal::Style active_tab;
    terminal::Style status_bar;
    terminal::Style status_bar_shortcut;

    [[nodiscard]] constexpr auto style(ThemeRole role) const noexcept -> const terminal::Style& {
        switch (role) {
        case ThemeRole::text:
            return text;
        case ThemeRole::title:
            return title;
        case ThemeRole::menu:
            return menu;
        case ThemeRole::menu_mnemonic:
            return menu_mnemonic;
        case ThemeRole::selection:
            return selection;
        case ThemeRole::popup:
            return popup;
        case ThemeRole::popup_selection:
            return popup_selection;
        case ThemeRole::active_menu:
            return active_menu;
        case ThemeRole::active_menu_mnemonic:
            return active_menu_mnemonic;
        case ThemeRole::tab:
            return tab;
        case ThemeRole::active_tab:
            return active_tab;
        case ThemeRole::status_bar:
            return status_bar;
        case ThemeRole::status_bar_shortcut:
            return status_bar_shortcut;
        }
        return text;
    }
};

[[nodiscard]] auto theme(ThemeName name) -> const Theme&;
[[nodiscard]] auto parse_theme(std::string_view name) -> ThemeName;

} // namespace vulpes::ui
