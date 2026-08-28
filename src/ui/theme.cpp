#include "vulpes/ui/theme.hpp"

#include "vulpes/core/error.hpp"

#include <cctype>
#include <string>

namespace vulpes::ui {
namespace {

[[nodiscard]] auto lowercase_ascii(std::string_view value) -> std::string {
    std::string result{value};
    for (auto& character : result)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return result;
}

} // namespace

auto theme(ThemeName name) -> const Theme& {
    static constexpr Theme midnight{
        .desktop = {.foreground = {215, 230, 245}, .background = {0, 28, 68}},
        .text = {.foreground = {225, 237, 250}, .background = {0, 38, 82}},
        .muted_text = {.foreground = {145, 175, 205}, .background = {0, 38, 82}},
        .title = {.foreground = {255, 234, 128}, .background = {0, 38, 82}, .bold = true},
        .border = {.foreground = {90, 210, 255}, .background = {0, 38, 82}},
        .shadow = {.foreground = {0, 12, 30}, .background = {0, 12, 30}},
        .input = {.foreground = {20, 28, 38}, .background = {225, 237, 250}},
        .input_focus = {.foreground = {0, 0, 0}, .background = {255, 220, 90}, .bold = true},
        .error = {.foreground = {255, 188, 188}, .background = {95, 20, 30}, .bold = true},
        .disabled = {.foreground = {105, 130, 155}, .background = {0, 25, 65}},
        .menu = {.foreground = {230, 242, 255}, .background = {0, 45, 110}, .bold = true},
        .menu_mnemonic = {.foreground = {255, 220, 90}, .background = {0, 45, 110}, .bold = true, .underline = true},
        .selection = {.foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true},
        .popup = {.foreground = {220, 235, 255}, .background = {0, 25, 65}},
        .popup_selection = {.foreground = {0, 0, 0}, .background = {255, 220, 90}, .bold = true},
        .active_menu = {.foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true},
        .active_menu_mnemonic = {.foreground = {0, 0, 0},
                                 .background = {95, 220, 255},
                                 .bold = true,
                                 .underline = true},
        .tab = {.foreground = {175, 200, 225}, .background = {0, 30, 72}},
        .active_tab = {.foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true},
        .grid_header = {.foreground = {255, 234, 128}, .background = {0, 52, 105}, .bold = true},
        .grid_cell = {.foreground = {225, 237, 250}, .background = {0, 38, 82}},
        .grid_selected_row = {.foreground = {255, 255, 255}, .background = {20, 86, 130}},
        .grid_selected_cell = {.foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true},
        .grid_footer = {.foreground = {175, 200, 225}, .background = {0, 30, 72}},
        .status_bar = {.foreground = {230, 242, 255}, .background = {0, 35, 82}},
        .status_bar_shortcut = {.foreground = {255, 220, 90}, .background = {0, 35, 82}, .bold = true},
    };
    static constexpr Theme high_contrast{
        .desktop = {.foreground = {255, 255, 255}, .background = {0, 0, 0}},
        .text = {.foreground = {255, 255, 255}, .background = {0, 0, 0}},
        .muted_text = {.foreground = {192, 192, 192}, .background = {0, 0, 0}},
        .title = {.foreground = {255, 255, 0}, .background = {0, 0, 0}, .bold = true},
        .border = {.foreground = {255, 255, 255}, .background = {0, 0, 0}, .bold = true},
        .shadow = {.foreground = {128, 128, 128}, .background = {0, 0, 0}},
        .input = {.foreground = {0, 0, 0}, .background = {255, 255, 255}},
        .input_focus = {.foreground = {0, 0, 0}, .background = {255, 255, 0}, .bold = true},
        .error = {.foreground = {255, 255, 255}, .background = {192, 0, 0}, .bold = true},
        .disabled = {.foreground = {128, 128, 128}, .background = {0, 0, 0}},
        .menu = {.foreground = {255, 255, 255}, .background = {0, 0, 0}, .bold = true},
        .menu_mnemonic = {.foreground = {255, 255, 0}, .background = {0, 0, 0}, .bold = true, .underline = true},
        .selection = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
        .popup = {.foreground = {255, 255, 255}, .background = {0, 0, 0}},
        .popup_selection = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
        .active_menu = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
        .active_menu_mnemonic = {.foreground = {0, 0, 0},
                                 .background = {255, 255, 255},
                                 .bold = true,
                                 .underline = true},
        .tab = {.foreground = {255, 255, 255}, .background = {0, 0, 0}, .underline = true},
        .active_tab = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
        .grid_header = {.foreground = {255, 255, 0}, .background = {0, 0, 0}, .bold = true},
        .grid_cell = {.foreground = {255, 255, 255}, .background = {0, 0, 0}},
        .grid_selected_row = {.foreground = {0, 0, 0}, .background = {192, 192, 192}, .bold = true},
        .grid_selected_cell = {.foreground = {0, 0, 0}, .background = {255, 255, 0}, .bold = true},
        .grid_footer = {.foreground = {255, 255, 255}, .background = {0, 0, 0}},
        .status_bar = {.foreground = {255, 255, 255}, .background = {0, 0, 0}},
        .status_bar_shortcut = {.foreground = {255, 255, 0}, .background = {0, 0, 0}, .bold = true, .underline = true},
    };

    return name == ThemeName::high_contrast ? high_contrast : midnight;
}

auto parse_theme(std::string_view name) -> ThemeName {
    const auto normalized = lowercase_ascii(name);
    if (normalized == "midnight")
        return ThemeName::midnight;
    if (normalized == "high-contrast" || normalized == "high_contrast")
        return ThemeName::high_contrast;
    throw Error{ErrorCategory::validation, "unknown theme '" + std::string{name} + "'; use midnight or high-contrast"};
}

} // namespace vulpes::ui
