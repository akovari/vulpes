#include "vulpes/core/localization.hpp"

#include "vulpes/core/error.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace vulpes::core {

Localizer::Localizer(std::string locale) : locale_{std::move(locale)} {
    add_catalog("en", english_catalog());
}

void Localizer::add_catalog(std::string locale, MessageCatalog catalog) {
    catalogs_.insert_or_assign(std::move(locale), std::move(catalog));
}

void Localizer::load_catalog_file(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        throw Error{ErrorCategory::io, "unable to inspect message catalog: " + path.string()};
    if (size > 1U * 1024U * 1024U)
        throw Error{ErrorCategory::metadata, "message catalog exceeds the 1 MiB size limit: " + path.string()};

    std::ifstream input{path, std::ios::binary};
    if (!input)
        throw Error{ErrorCategory::io, "unable to open message catalog: " + path.string()};

    try {
        const auto document = nlohmann::json::parse(input);
        if (!document.is_object() || !document.contains("locale") || !document.at("locale").is_string() ||
            !document.contains("messages") || !document.at("messages").is_object()) {
            throw Error{ErrorCategory::metadata, "message catalog must contain string locale and object messages"};
        }

        const auto locale = document.at("locale").get<std::string>();
        if (locale.empty())
            throw Error{ErrorCategory::metadata, "message catalog locale cannot be empty"};

        MessageCatalog catalog;
        for (const auto& [key, value] : document.at("messages").items()) {
            if (key.empty() || !value.is_string())
                throw Error{ErrorCategory::metadata, "message catalog keys and values must be non-empty strings"};
            catalog.insert_or_assign(key, value.get<std::string>());
        }
        add_catalog(locale, std::move(catalog));
    } catch (const nlohmann::json::exception& exception) {
        throw Error{ErrorCategory::metadata,
                    "unable to parse message catalog '" + path.string() + "': " + exception.what()};
    }
}

auto Localizer::find_template(std::string_view key) const -> const std::string* {
    const auto locate = [&](std::string_view locale) -> const std::string* {
        const auto catalog = catalogs_.find(std::string{locale});
        if (catalog == catalogs_.end())
            return nullptr;
        const auto message = catalog->second.find(std::string{key});
        return message == catalog->second.end() ? nullptr : &message->second;
    };
    if (const auto result = locate(locale_))
        return result;
    if (const auto separator = locale_.find('-'); separator != std::string::npos) {
        if (const auto result = locate(std::string_view{locale_}.substr(0, separator)))
            return result;
    }
    return locate("en");
}

auto Localizer::format(std::string_view text, const MessageArguments& arguments) -> std::string {
    std::string result;
    std::size_t position = 0;
    while (position < text.size()) {
        const auto open = text.find('{', position);
        if (open == std::string_view::npos) {
            result.append(text.substr(position));
            break;
        }
        result.append(text.substr(position, open - position));
        const auto close = text.find('}', open + 1);
        if (close == std::string_view::npos) {
            result.append(text.substr(open));
            break;
        }
        const auto key = text.substr(open + 1, close - open - 1);
        if (const auto argument = arguments.find(key); argument != arguments.end())
            result += argument->second;
        else
            result.append(text.substr(open, close - open + 1));
        position = close + 1;
    }
    return result;
}

auto Localizer::translate(std::string_view key, const MessageArguments& arguments) const -> std::string {
    if (const auto text = find_template(key))
        return format(*text, arguments);
    return std::string{key};
}

auto english_catalog() -> MessageCatalog {
    return {
        {"application.title", "Vulpes"},
        {"application.unknown_command", "Unknown command. Type help for available commands."},
        {"terminal.diagnostics.instructions", "Press keys to inspect normalized input. Esc or Ctrl+C exits."},
        {"terminal.diagnostics.title", "Terminal diagnostics"},
        {"terminal.diagnostics.waiting", "Waiting for a terminal event..."},
        {"terminal.capabilities.available", "Interactive TUI available"},
        {"terminal.capabilities.connected", "terminal"},
        {"terminal.capabilities.input", "Standard input"},
        {"terminal.capabilities.no", "no"},
        {"terminal.capabilities.output", "Standard output"},
        {"terminal.capabilities.redirected", "redirected or unavailable"},
        {"terminal.capabilities.title", "Terminal capabilities"},
        {"terminal.capabilities.yes", "yes"},
        {"browse.filter_prompt", "Filter {field} (for example: >= 10; blank clears filters)"},
        {"browse.delete_message", "Delete the selected record from {table}?"},
        {"browse.delete_title", "Delete record"},
        {"browse.footer", "F2 Edit  Ins New  Del Delete  F3 Search  F4 Filter  F5 Refresh  F6 Sort  Esc Back"},
        {"browse.read_only_footer", "F3 Search  F4 Filter  F5 Refresh  F6 Sort  Esc Back  [read-only]"},
        {"browse.search_prompt", "Search text (blank clears search)"},
        {"dialog.cancel", "Cancel"},
        {"dialog.delete", "Delete"},
        {"dialog.select", "Left/Right Select   Enter Apply   Esc Cancel"},
        {"form.instructions", "F8 Save   Esc Cancel   Left/Right Cursor/Lookup   Home/End"},
        {"form.edit_title", "Edit {table}"},
        {"form.new_title", "New {table}"},
        {"prompt.instructions", "Left/Right Move   Home/End   Enter Apply   Esc Cancel"},
        {"command.help", "Commands: help, tables, schema <table>, browse <table>, sql, quit"},
        {"database.tables", "Tables and views"},
        {"database.view_suffix", " [view]"},
        {"sql.empty_error", "Enter a SQL statement before executing."},
        {"sql.instructions", "Arrows Move   Home/End   Enter New line   Tab Indent   F7 Pane   F8 Execute   Esc Back"},
        {"sql.result_footer", "F7 Focus results   Arrows Navigate   F8 Execute again"},
        {"sql.results", "SQL results"},
        {"sql.status", "Executed: {rows} row(s), {changes} change(s){truncated}."},
        {"sql.title", "SQL console"},
        {"sql.truncated", " (result truncated)"},
        {"error.invalid_command_arguments", "Invalid arguments for '{command}'."},
        {"error.unknown_table", "No table or view named '{name}'."},
        {"schema.generated", "generated"},
        {"schema.footer", "Up/Down Navigate   Esc Back"},
        {"schema.not_null", "not null"},
        {"schema.primary_key", "primary key"},
        {"schema.title", "Schema: {name}"},
        {"schema.unique", "unique"},
        {"terminal.minimum_size", "Terminal is too small. Resize to at least {width} x {height}. Esc or Ctrl+C exits."},
        {"workspace.database_status", "{path} — {count} table(s) and view(s)"},
        {"workspace.read_only_suffix", " [read-only]"},
        {"workspace.command_database_required", "Open a database before running a command."},
        {"workspace.command_instructions", "Left/Right Move   Home/End   Enter Execute   Esc Cancel   Type help"},
        {"workspace.command_tables", "Refreshed {count} table(s) and view(s)."},
        {"workspace.command_title", "Command"},
        {"workspace.close_document_cancel", "Cancel"},
        {"workspace.close_document_confirm", "Close"},
        {"workspace.close_document_instructions", "Left/Right Select   Enter Apply   Esc Cancel"},
        {"workspace.close_document_message", "Close the {title} document?"},
        {"workspace.close_document_title", "Close document"},
        {"workspace.document.browse", "Browse {table}"},
        {"workspace.document.schema", "Schema: {table}"},
        {"workspace.document.sql", "SQL"},
        {"workspace.document.workspace", "Workspace"},
        {"workspace.directory_browser_instructions", "Enter Open/select directory   Backspace Parent   Esc Cancel"},
        {"workspace.directory_browser_parent", "[..]"},
        {"workspace.directory_browser_title", "Browse files"},
        {"workspace.home_shortcuts",
         "Ctrl+O Open database   Ctrl+R Open read-only   Ctrl+N Create database   Ctrl+P Command   F10 Menu"},
        {"workspace.help_shortcuts", "F10 menu  Alt+F File  Ctrl+O open  Ctrl+R open read-only  Ctrl+N create  Ctrl+P "
                                     "command  Ctrl+Tab next tab  Ctrl+W close tab"},
        {"workspace.menu.file.create", "Create database"},
        {"workspace.menu.file.exit", "Exit"},
        {"workspace.menu.file.open", "Open database"},
        {"workspace.menu.file.open_read_only", "Open database read-only"},
        {"workspace.menu.file.browse_files", "Browse files..."},
        {"workspace.menu.database.browse", "Browse selected table"},
        {"workspace.menu.database.browse_files", "Browse files..."},
        {"workspace.menu.database.create", "Create database"},
        {"workspace.menu.database.open", "Open database"},
        {"workspace.menu.database.open_read_only", "Open database read-only"},
        {"workspace.menu.database.sql", "SQL console"},
        {"workspace.menu.help.shortcuts", "Keyboard shortcuts"},
        {"workspace.menu.view.next", "Next table"},
        {"workspace.menu.view.previous", "Previous table"},
        {"workspace.menu.window.close", "Close document"},
        {"workspace.menu.window.next", "Next document"},
        {"workspace.menu_bar.database", "Database"},
        {"workspace.menu_bar.file", "File"},
        {"workspace.menu_bar.help", "Help"},
        {"workspace.menu_bar.view", "View"},
        {"workspace.menu_bar.window", "Window"},
        {"workspace.no_database_open", "No database open."},
        {"workspace.open_before_browse", "Open a database before browsing a table."},
        {"workspace.open_before_sql", "Open a database before using the SQL console."},
        {"workspace.open_title", "Open SQLite database"},
        {"workspace.open_read_only_title", "Open SQLite database read-only"},
        {"workspace.create_title", "Create SQLite database"},
        {"workspace.path_instructions", "Left/Right Move   Home/End   Enter Apply   Esc Cancel"},
        {"workspace.path_required", "A database path is required."},
        {"workspace.recent_databases", "Recent databases:"},
        {"workspace.shortcut.create.key", "Ctrl+N"},
        {"workspace.shortcut.create.label", " Create  "},
        {"workspace.shortcut.menu.key", "F10"},
        {"workspace.shortcut.menu.label", " Menu  "},
        {"workspace.shortcut.open.key", "Ctrl+O"},
        {"workspace.shortcut.open.label", " Open  "},
        {"workspace.shortcut.quit.key", "Ctrl+C"},
        {"workspace.shortcut.quit.label", " Exit"},
        {"workspace.shortcut.command.key", "Ctrl+P"},
        {"workspace.shortcut.command.label", " Command  "},
        {"workspace.tables_and_views", "Tables and views:"},
        {"workspace.view_suffix", " [view]"},
    };
}

} // namespace vulpes::core
