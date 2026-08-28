#pragma once

#include "vulpes/terminal/screen_buffer.hpp"

#include <string_view>

namespace vulpes::ui {

// Roles describe intent rather than a screen's RGB values. Terminal renderers
// remain free to reduce these styles to their own capability set.
enum class ThemeRole {
    desktop,
    text,
    muted_text,
    title,
    border,
    shadow,
    input,
    input_focus,
    error,
    disabled,
    menu,
    menu_mnemonic,
    selection,
    popup,
    popup_selection,
    active_menu,
    active_menu_mnemonic,
    tab,
    active_tab,
    grid_header,
    grid_cell,
    grid_selected_row,
    grid_selected_cell,
    grid_footer,
    status_bar,
    status_bar_shortcut,
};

enum class ThemeName { midnight, high_contrast };

struct Theme {
    terminal::Style desktop;
    terminal::Style text;
    terminal::Style muted_text;
    terminal::Style title;
    terminal::Style border;
    terminal::Style shadow;
    terminal::Style input;
    terminal::Style input_focus;
    terminal::Style error;
    terminal::Style disabled;
    terminal::Style menu;
    terminal::Style menu_mnemonic;
    terminal::Style selection;
    terminal::Style popup;
    terminal::Style popup_selection;
    terminal::Style active_menu;
    terminal::Style active_menu_mnemonic;
    terminal::Style tab;
    terminal::Style active_tab;
    terminal::Style grid_header;
    terminal::Style grid_cell;
    terminal::Style grid_selected_row;
    terminal::Style grid_selected_cell;
    terminal::Style grid_footer;
    terminal::Style status_bar;
    terminal::Style status_bar_shortcut;

    [[nodiscard]] constexpr auto style(ThemeRole role) const noexcept -> const terminal::Style& {
        switch (role) {
        case ThemeRole::desktop:
            return desktop;
        case ThemeRole::text:
            return text;
        case ThemeRole::muted_text:
            return muted_text;
        case ThemeRole::title:
            return title;
        case ThemeRole::border:
            return border;
        case ThemeRole::shadow:
            return shadow;
        case ThemeRole::input:
            return input;
        case ThemeRole::input_focus:
            return input_focus;
        case ThemeRole::error:
            return error;
        case ThemeRole::disabled:
            return disabled;
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
        case ThemeRole::grid_header:
            return grid_header;
        case ThemeRole::grid_cell:
            return grid_cell;
        case ThemeRole::grid_selected_row:
            return grid_selected_row;
        case ThemeRole::grid_selected_cell:
            return grid_selected_cell;
        case ThemeRole::grid_footer:
            return grid_footer;
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
