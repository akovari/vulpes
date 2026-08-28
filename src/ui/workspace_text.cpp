#include "vulpes/ui/workspace_text.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <span>
#include <unordered_set>

namespace vulpes::ui {
namespace {

[[nodiscard]] auto mnemonic(const core::Localizer& messages, std::string_view key) -> char32_t {
    const auto source = messages.translate(key);
    const auto code_point = terminal::first_code_point(source);
    if (code_point == U'\0' || terminal::encode_utf8(code_point) != source)
        throw Error{ErrorCategory::metadata, "menu mnemonic '" + std::string{key} + "' must contain one code point"};
    return code_point;
}

void validate_mnemonics(std::string_view scope, std::span<const std::string> labels,
                        std::span<const char32_t> mnemonics) {
    if (labels.size() != mnemonics.size())
        throw Error{ErrorCategory::metadata, "menu mnemonic count does not match labels in " + std::string{scope}};
    std::unordered_set<char32_t> assigned;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (!terminal::find_code_point_column(labels[index], mnemonics[index]))
            throw Error{ErrorCategory::metadata,
                        "menu mnemonic is not present in label '" + labels[index] + "' in " + std::string{scope}};
        const auto normalized = terminal::lowercase_code_point(mnemonics[index]);
        if (!assigned.insert(normalized).second)
            throw Error{ErrorCategory::metadata, "duplicate menu mnemonic in " + std::string{scope}};
    }
}

} // namespace

auto make_workspace_text(const core::Localizer& messages) -> WorkspaceText {
    WorkspaceText text{
        .title = messages.translate("application.title"),
        .workspace_document = messages.translate("workspace.document.workspace"),
        .open_database_title = messages.translate("workspace.open_title"),
        .open_read_only_database_title = messages.translate("workspace.open_read_only_title"),
        .create_database_title = messages.translate("workspace.create_title"),
        .directory_browser_title = messages.translate("workspace.directory_browser_title"),
        .directory_browser_instructions = messages.translate("workspace.directory_browser_instructions"),
        .directory_browser_parent = messages.translate("workspace.directory_browser_parent"),
        .path_instructions = messages.translate("workspace.path_instructions"),
        .command_title = messages.translate("workspace.command_title"),
        .command_instructions = messages.translate("workspace.command_instructions"),
        .close_document_title = messages.translate("workspace.close_document_title"),
        .close_document_message = messages.bind("workspace.close_document_message"),
        .close_document_confirm = messages.translate("workspace.close_document_confirm"),
        .close_document_cancel = messages.translate("workspace.close_document_cancel"),
        .close_document_instructions = messages.translate("workspace.close_document_instructions"),
        .no_database_open = messages.translate("workspace.no_database_open"),
        .open_before_browse = messages.translate("workspace.open_before_browse"),
        .open_before_sql = messages.translate("workspace.open_before_sql"),
        .recent_databases = messages.translate("workspace.recent_databases"),
        .tables_and_views = messages.translate("workspace.tables_and_views"),
        .home_shortcuts = messages.translate("workspace.home_shortcuts"),
        .help_shortcuts = messages.translate("workspace.help_shortcuts"),
        .database_status = messages.bind("workspace.database_status"),
        .read_only_suffix = messages.translate("workspace.access_mode", {{"mode", "read_only"}}),
        .browse_document = messages.bind("workspace.document.browse"),
        .schema_document = messages.bind("workspace.document.schema"),
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
        .menu_bar_mnemonics =
            {
                mnemonic(messages, "workspace.menu_bar.file.mnemonic"),
                mnemonic(messages, "workspace.menu_bar.database.mnemonic"),
                mnemonic(messages, "workspace.menu_bar.view.mnemonic"),
                mnemonic(messages, "workspace.menu_bar.window.mnemonic"),
                mnemonic(messages, "workspace.menu_bar.help.mnemonic"),
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
                ShortcutHint{.key = messages.translate("workspace.shortcut.command.key"),
                             .label = messages.translate("workspace.shortcut.command.label")},
            },
        .file_menu =
            {
                messages.translate("workspace.menu.file.open"),
                messages.translate("workspace.menu.file.open_read_only"),
                messages.translate("workspace.menu.file.browse_files"),
                messages.translate("workspace.menu.file.create"),
                messages.translate("workspace.menu.file.exit"),
            },
        .file_menu_mnemonics =
            {
                mnemonic(messages, "workspace.menu.file.open.mnemonic"),
                mnemonic(messages, "workspace.menu.file.open_read_only.mnemonic"),
                mnemonic(messages, "workspace.menu.file.browse_files.mnemonic"),
                mnemonic(messages, "workspace.menu.file.create.mnemonic"),
                mnemonic(messages, "workspace.menu.file.exit.mnemonic"),
            },
        .file_menu_shortcuts = {"Ctrl+O", "Ctrl+R", "", "Ctrl+N", "Ctrl+C"},
        .database_menu =
            {
                messages.translate("workspace.menu.database.open"),
                messages.translate("workspace.menu.database.open_read_only"),
                messages.translate("workspace.menu.database.browse_files"),
                messages.translate("workspace.menu.database.create"),
                messages.translate("workspace.menu.database.browse"),
                messages.translate("workspace.menu.database.sql"),
            },
        .database_menu_mnemonics =
            {
                mnemonic(messages, "workspace.menu.database.open.mnemonic"),
                mnemonic(messages, "workspace.menu.database.open_read_only.mnemonic"),
                mnemonic(messages, "workspace.menu.database.browse_files.mnemonic"),
                mnemonic(messages, "workspace.menu.database.create.mnemonic"),
                mnemonic(messages, "workspace.menu.database.browse.mnemonic"),
                mnemonic(messages, "workspace.menu.database.sql.mnemonic"),
            },
        .database_menu_shortcuts = {"Ctrl+O", "Ctrl+R", "", "Ctrl+N", "Enter", "F7"},
        .view_menu =
            {
                messages.translate("workspace.menu.view.previous"),
                messages.translate("workspace.menu.view.next"),
            },
        .view_menu_mnemonics =
            {
                mnemonic(messages, "workspace.menu.view.previous.mnemonic"),
                mnemonic(messages, "workspace.menu.view.next.mnemonic"),
            },
        .view_menu_shortcuts = {"Up", "Down"},
        .window_menu =
            {
                messages.translate("workspace.menu.window.next"),
                messages.translate("workspace.menu.window.close"),
            },
        .window_menu_mnemonics =
            {
                mnemonic(messages, "workspace.menu.window.next.mnemonic"),
                mnemonic(messages, "workspace.menu.window.close.mnemonic"),
            },
        .window_menu_shortcuts = {"Ctrl+Tab", "Ctrl+W"},
        .help_menu = {messages.translate("workspace.menu.help.shortcuts")},
        .help_menu_mnemonics = {mnemonic(messages, "workspace.menu.help.shortcuts.mnemonic")},
        .help_menu_shortcuts = {""},
    };
    validate_mnemonics("menu bar", text.menu_bar, text.menu_bar_mnemonics);
    validate_mnemonics("File menu", text.file_menu, text.file_menu_mnemonics);
    validate_mnemonics("Database menu", text.database_menu, text.database_menu_mnemonics);
    validate_mnemonics("View menu", text.view_menu, text.view_menu_mnemonics);
    validate_mnemonics("Window menu", text.window_menu, text.window_menu_mnemonics);
    validate_mnemonics("Help menu", text.help_menu, text.help_menu_mnemonics);
    return text;
}

} // namespace vulpes::ui
