#include "vulpes/core/localization.hpp"

#include "vulpes/core/error.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <type_traits>
#include <unicode/fmtable.h>
#include <unicode/locid.h>
#include <unicode/msgfmt.h>
#include <unicode/parseerr.h>
#include <unicode/stringpiece.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>
#include <utility>
#include <vector>

namespace vulpes::core {
namespace {

[[nodiscard]] auto unicode(std::string_view text) -> icu::UnicodeString {
    return icu::UnicodeString::fromUTF8(icu::StringPiece{text.data(), static_cast<std::int32_t>(text.size())});
}

[[nodiscard]] auto locale_for(std::string_view name, ErrorCategory category) -> icu::Locale {
    UErrorCode status = U_ZERO_ERROR;
    auto locale =
        icu::Locale::forLanguageTag(icu::StringPiece{name.data(), static_cast<std::int32_t>(name.size())}, status);
    if (U_FAILURE(status) || locale.isBogus())
        throw Error{category, "invalid BCP-47 locale '" + std::string{name} + "': " + u_errorName(status),
                    static_cast<int>(status)};
    return locale;
}

[[nodiscard]] auto canonical_locale(std::string_view name, ErrorCategory category) -> std::string {
    UErrorCode status = U_ZERO_ERROR;
    const auto locale = locale_for(name, category);
    auto tag = locale.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || tag.empty())
        throw Error{category,
                    "unable to canonicalize BCP-47 locale '" + std::string{name} + "': " + u_errorName(status),
                    static_cast<int>(status)};
    return tag;
}

[[nodiscard]] auto formattable(const MessageValue& value) -> icu::Formattable {
    return std::visit(
        [](const auto& item) -> icu::Formattable {
            using Value = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::string>)
                return icu::Formattable{unicode(item)};
            else if constexpr (std::is_same_v<Value, std::int64_t> || std::is_same_v<Value, double>)
                return icu::Formattable{item};
            else
                return icu::Formattable{unicode(item ? "true" : "false")};
        },
        value.storage());
}

[[nodiscard]] auto format_message(std::string_view pattern, std::string_view locale_name,
                                  const MessageArguments& arguments, ErrorCategory category) -> std::string {
    UErrorCode status = U_ZERO_ERROR;
    UParseError parse_error{};
    icu::MessageFormat formatter{unicode(pattern), locale_for(locale_name, category), parse_error, status};
    if (U_FAILURE(status)) {
        throw Error{category,
                    "invalid ICU message pattern at offset " + std::to_string(parse_error.offset) + ": " +
                        u_errorName(status),
                    static_cast<int>(status)};
    }

    std::vector<icu::UnicodeString> names;
    std::vector<icu::Formattable> values;
    names.reserve(arguments.size());
    values.reserve(arguments.size());
    for (const auto& [name, value] : arguments) {
        names.push_back(unicode(name));
        values.push_back(formattable(value));
    }

    icu::UnicodeString output;
    formatter.format(names.data(), values.data(), static_cast<std::int32_t>(values.size()), output, status);
    if (U_FAILURE(status))
        throw Error{category, "unable to format localized message: " + std::string{u_errorName(status)},
                    static_cast<int>(status)};
    std::string result;
    output.toUTF8String(result);
    return result;
}

void validate_pattern(std::string_view pattern, std::string_view locale, std::string_view key) {
    UErrorCode status = U_ZERO_ERROR;
    UParseError parse_error{};
    const icu::MessageFormat formatter{unicode(pattern), locale_for(locale, ErrorCategory::metadata), parse_error,
                                       status};
    if (U_FAILURE(status)) {
        throw Error{ErrorCategory::metadata,
                    "invalid message pattern '" + std::string{key} + "' for locale '" + std::string{locale} +
                        "' at offset " + std::to_string(parse_error.offset) + ": " + u_errorName(status),
                    static_cast<int>(status)};
    }
}

void validate_catalog(std::string_view locale, const MessageCatalog& catalog) {
    static_cast<void>(locale_for(locale, ErrorCategory::metadata));
    for (const auto& [key, pattern] : catalog)
        validate_pattern(pattern, locale, key);
}

} // namespace

Localizer::Localizer(std::string locale) : locale_{canonical_locale(locale, ErrorCategory::validation)} {
    add_catalog("en", english_catalog());
}

void Localizer::add_catalog(std::string locale, MessageCatalog catalog) {
    locale = canonical_locale(locale, ErrorCategory::metadata);
    validate_catalog(locale, catalog);
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

auto Localizer::find_template(std::string_view key) const -> ResolvedMessage {
    const auto locate = [&](std::string_view locale, std::string formatting_locale) -> ResolvedMessage {
        const auto catalog = catalogs_.find(std::string{locale});
        if (catalog == catalogs_.end())
            return {};
        const auto message = catalog->second.find(std::string{key});
        return message == catalog->second.end() ? ResolvedMessage{}
                                                : ResolvedMessage{&message->second, std::move(formatting_locale)};
    };
    if (const auto result = locate(locale_, locale_); result.pattern != nullptr)
        return result;
    if (const auto separator = locale_.find('-'); separator != std::string::npos) {
        if (const auto result = locate(std::string_view{locale_}.substr(0, separator), locale_);
            result.pattern != nullptr) {
            return result;
        }
    }
    return locate("en", locale_.starts_with("en") ? locale_ : "en");
}

auto LocalizedMessage::format(const MessageArguments& arguments) const -> std::string {
    return format_message(pattern_, locale_, arguments, ErrorCategory::metadata);
}

auto Localizer::bind(std::string_view key) const -> LocalizedMessage {
    if (const auto resolved = find_template(key); resolved.pattern != nullptr)
        return LocalizedMessage{*resolved.pattern, resolved.locale};
    return LocalizedMessage{std::string{key}, "en"};
}

auto Localizer::translate(std::string_view key, const MessageArguments& arguments) const -> std::string {
    return bind(key).format(arguments);
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
        {"browse.footer", "F2 Edit  Ins New  Del Delete  F3 Search  F4 Filter  F5 Refresh  F6 Sort  Ctrl+←/→ Size"},
        {"browse.read_only_footer", "F3 Search  F4 Filter  F5 Refresh  F6 Sort  Ctrl+←/→ Size  [read-only]"},
        {"browse.search_prompt", "Search text (blank clears search)"},
        {"grid.empty", "No records"},
        {"grid.row", "Row"},
        {"grid.rows", "Rows"},
        {"grid.column", "Col"},
        {"dialog.cancel", "Cancel"},
        {"dialog.delete", "Delete"},
        {"dialog.select", "Left/Right Select   Enter Apply   Esc Cancel"},
        {"form.instructions", "Enter Lookup   F8 Save   Esc Cancel   Shift+Arrows Select   Ins Overwrite"},
        {"form.edit_title", "Edit {table}"},
        {"form.new_title", "New {table}"},
        {"lookup.instructions", "Type to search   Up/Down Select   Enter Apply   F2 View   Esc Cancel"},
        {"lookup.related_instructions", "Up/Down Navigate   Esc Back"},
        {"lookup.related_title", "Related {table}"},
        {"lookup.search", "Search:"},
        {"lookup.title", "Select {field}"},
        {"prompt.instructions", "Arrows Move   Shift+Arrows Select   Enter Apply   Esc Cancel"},
        {"command.help",
         "Commands: help, tables, schema <table>, browse <table>, forms, form <name>, screens, screen <name>, views, "
         "view <name>, reports, report <name>, export <report> <format> <path> [overwrite], run <command>, sql, quit"},
        {"export.complete", "{rows, plural, one {Exported # row} other {Exported # rows}} to {path} ({format})."},
        {"export.query_title", "SQL query result"},
        {"report.footer", "Esc Back  Arrow keys Navigate  F6 Sort"},
        {"report.footer_truncated", "Result truncated  Esc Back  Arrow keys Navigate  F6 Sort"},
        {"workspace.command_forms", "{count, plural, one {# form} other {# forms}}"},
        {"workspace.command_screens", "{count, plural, one {# screen} other {# screens}}"},
        {"workspace.command_views", "{count, plural, one {# view} other {# views}}"},
        {"workspace.command_reports", "{count, plural, one {# report} other {# reports}}"},
        {"error.unknown_definition", "Unknown application item: {name}"},
        {"error.command_cycle", "Application command recursion limit reached."},
        {"database.tables", "Tables and views"},
        {"database.view_suffix", " [view]"},
        {"sql.empty_error", "Enter a SQL statement before executing."},
        {"sql.instructions", "Arrows Move   Ctrl+Up/Down History   F7 Pane   F8 Execute   Esc Back"},
        {"sql.result_footer", "F7 Results   Arrows Navigate   Ctrl+←/→ Size   F8 Execute"},
        {"sql.results", "SQL results"},
        {"sql.status", "Executed: {rows} row(s), {changes} change(s){truncated}."},
        {"sql.title", "SQL console"},
        {"sql.truncated", " (result truncated)"},
        {"error.invalid_command_arguments", "Invalid arguments for ‘{command}’."},
        {"error.unknown_table", "No table or view named ‘{name}’."},
        {"schema.generated", "generated"},
        {"schema.footer", "Up/Down Navigate   Esc Back"},
        {"schema.not_null", "not null"},
        {"schema.primary_key", "primary key"},
        {"schema.title", "Schema: {name}"},
        {"schema.unique", "unique"},
        {"terminal.minimum_size", "Terminal is too small. Resize to at least {width} x {height}. Esc or Ctrl+C exits."},
        {"workspace.database_status",
         "{count, plural, one {{path} — # table or view} other {{path} — # tables and views}}"},
        {"workspace.access_mode", "{mode, select, read_only { [read-only]} other {}}"},
        {"workspace.command_database_required", "Open a database before running a command."},
        {"workspace.command_instructions", "Left/Right Move   Home/End   Enter Execute   Esc Cancel   Type help"},
        {"workspace.command_tables",
         "{count, plural, one {Refreshed # table or view.} other {Refreshed # tables and views.}}"},
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
        {"workspace.menu.file.create.mnemonic", "C"},
        {"workspace.menu.file.exit", "Exit"},
        {"workspace.menu.file.exit.mnemonic", "E"},
        {"workspace.menu.file.open", "Open database"},
        {"workspace.menu.file.open.mnemonic", "O"},
        {"workspace.menu.file.open_read_only", "Open database read-only"},
        {"workspace.menu.file.open_read_only.mnemonic", "R"},
        {"workspace.menu.file.browse_files", "Browse files..."},
        {"workspace.menu.file.browse_files.mnemonic", "B"},
        {"workspace.menu.database.browse", "Browse selected table"},
        {"workspace.menu.database.browse.mnemonic", "B"},
        {"workspace.menu.database.browse_files", "Browse files..."},
        {"workspace.menu.database.browse_files.mnemonic", "F"},
        {"workspace.menu.database.create", "Create database"},
        {"workspace.menu.database.create.mnemonic", "C"},
        {"workspace.menu.database.open", "Open database"},
        {"workspace.menu.database.open.mnemonic", "O"},
        {"workspace.menu.database.open_read_only", "Open database read-only"},
        {"workspace.menu.database.open_read_only.mnemonic", "R"},
        {"workspace.menu.database.sql", "SQL console"},
        {"workspace.menu.database.sql.mnemonic", "S"},
        {"workspace.menu.help.shortcuts", "Keyboard shortcuts"},
        {"workspace.menu.help.shortcuts.mnemonic", "K"},
        {"workspace.menu.view.next", "Next table"},
        {"workspace.menu.view.next.mnemonic", "N"},
        {"workspace.menu.view.previous", "Previous table"},
        {"workspace.menu.view.previous.mnemonic", "P"},
        {"workspace.menu.window.close", "Close document"},
        {"workspace.menu.window.close.mnemonic", "C"},
        {"workspace.menu.window.next", "Next document"},
        {"workspace.menu.window.next.mnemonic", "N"},
        {"workspace.menu_bar.database", "Database"},
        {"workspace.menu_bar.database.mnemonic", "D"},
        {"workspace.menu_bar.file", "File"},
        {"workspace.menu_bar.file.mnemonic", "F"},
        {"workspace.menu_bar.help", "Help"},
        {"workspace.menu_bar.help.mnemonic", "H"},
        {"workspace.menu_bar.view", "View"},
        {"workspace.menu_bar.view.mnemonic", "V"},
        {"workspace.menu_bar.window", "Window"},
        {"workspace.menu_bar.window.mnemonic", "W"},
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
