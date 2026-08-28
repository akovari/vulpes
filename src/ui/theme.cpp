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
        .text = {},
        .title = {.foreground = {90, 210, 255}, .bold = true},
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
        .tab = {.foreground = {185, 210, 240}, .background = {0, 35, 82}},
        .active_tab = {.foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true},
    };
    static constexpr Theme high_contrast{
        .text = {},
        .title = {.foreground = {255, 255, 0}, .bold = true},
        .menu = {.foreground = {255, 255, 255}, .bold = true},
        .menu_mnemonic = {.foreground = {255, 255, 0}, .bold = true, .underline = true},
        .selection = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
        .popup = {.foreground = {255, 255, 255}},
        .popup_selection = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
        .active_menu = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
        .active_menu_mnemonic = {.foreground = {0, 0, 0},
                                 .background = {255, 255, 255},
                                 .bold = true,
                                 .underline = true},
        .tab = {.foreground = {255, 255, 255}, .underline = true},
        .active_tab = {.foreground = {0, 0, 0}, .background = {255, 255, 255}, .bold = true},
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
