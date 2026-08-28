#include "vulpes/ui/workspace_text.hpp"

namespace vulpes::ui {

auto make_workspace_text(const core::Localizer& messages) -> WorkspaceText {
    return {
        .title = messages.translate("application.title"),
        .workspace_document = messages.translate("workspace.document.workspace"),
        .open_database_title = messages.translate("workspace.open_title"),
        .create_database_title = messages.translate("workspace.create_title"),
        .path_instructions = messages.translate("workspace.path_instructions"),
        .no_database_open = messages.translate("workspace.no_database_open"),
        .open_before_browse = messages.translate("workspace.open_before_browse"),
        .open_before_sql = messages.translate("workspace.open_before_sql"),
        .tables_and_views = messages.translate("workspace.tables_and_views"),
        .home_shortcuts = messages.translate("workspace.home_shortcuts"),
        .help_shortcuts = messages.translate("workspace.help_shortcuts"),
        .database_status = messages.translate("workspace.database_status"),
        .browse_document = messages.translate("workspace.document.browse"),
        .sql_document = messages.translate("workspace.document.sql"),
        .view_suffix = messages.translate("workspace.view_suffix"),
        .menu_bar =
            {
                messages.translate("workspace.menu_bar.file"),
                messages.translate("workspace.menu_bar.database"),
                messages.translate("workspace.menu_bar.view"),
                messages.translate("workspace.menu_bar.window"),
                messages.translate("workspace.menu_bar.help"),
            },
        .status_shortcuts =
            {
                ShortcutHint{.key = messages.translate("workspace.shortcut.menu.key"),
                             .label = messages.translate("workspace.shortcut.menu.label")},
                ShortcutHint{.key = messages.translate("workspace.shortcut.open.key"),
                             .label = messages.translate("workspace.shortcut.open.label")},
                ShortcutHint{.key = messages.translate("workspace.shortcut.create.key"),
                             .label = messages.translate("workspace.shortcut.create.label")},
                ShortcutHint{.key = messages.translate("workspace.shortcut.quit.key"),
                             .label = messages.translate("workspace.shortcut.quit.label")},
            },
        .file_menu =
            {
                messages.translate("workspace.menu.file.open"),
                messages.translate("workspace.menu.file.create"),
                messages.translate("workspace.menu.file.exit"),
            },
        .database_menu =
            {
                messages.translate("workspace.menu.database.open"),
                messages.translate("workspace.menu.database.create"),
                messages.translate("workspace.menu.database.browse"),
                messages.translate("workspace.menu.database.sql"),
            },
        .view_menu =
            {
                messages.translate("workspace.menu.view.previous"),
                messages.translate("workspace.menu.view.next"),
            },
        .window_menu =
            {
                messages.translate("workspace.menu.window.next"),
                messages.translate("workspace.menu.window.close"),
            },
        .help_menu = {messages.translate("workspace.menu.help.shortcuts")},
    };
}

} // namespace vulpes::ui
