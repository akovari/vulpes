#pragma once

#include "vulpes/core/localization.hpp"

#include <array>
#include <string>

namespace vulpes::ui {

struct ShortcutHint {
    std::string key;
    std::string label;
};

// All visible workspace chrome is supplied by the presentation boundary. The
// workspace itself remains independent of catalog storage and locale policy.
struct WorkspaceText {
    std::string title;
    std::string workspace_document;
    std::string open_database_title;
    std::string create_database_title;
    std::string path_instructions;
    std::string command_title;
    std::string command_instructions;
    std::string no_database_open;
    std::string open_before_browse;
    std::string open_before_sql;
    std::string tables_and_views;
    std::string home_shortcuts;
    std::string help_shortcuts;
    std::string database_status;
    std::string browse_document;
    std::string schema_document;
    std::string sql_document;
    std::string view_suffix;
    std::array<std::string, 5> menu_bar;
    std::array<ShortcutHint, 5> status_shortcuts;
    std::array<std::string, 3> file_menu;
    std::array<std::string, 4> database_menu;
    std::array<std::string, 2> view_menu;
    std::array<std::string, 2> window_menu;
    std::array<std::string, 1> help_menu;
};

[[nodiscard]] auto make_workspace_text(const core::Localizer& messages) -> WorkspaceText;

} // namespace vulpes::ui
