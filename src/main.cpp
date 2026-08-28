#include "vulpes/core/actions.hpp"
#include "vulpes/core/application.hpp"
#include "vulpes/core/command.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/core/workspace_preferences.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/capabilities.hpp"
#include "vulpes/terminal/console_terminal.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/browse_document.hpp"
#include "vulpes/ui/document_session.hpp"
#include "vulpes/ui/schema_document.hpp"
#include "vulpes/ui/sql_document.hpp"
#include "vulpes/ui/terminal_diagnostics.hpp"
#include "vulpes/ui/terminal_warning.hpp"
#include "vulpes/ui/theme.hpp"
#include "vulpes/ui/workspace.hpp"
#include "vulpes/version.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void initialize_console_encoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

auto path_text(const std::filesystem::path& path) -> std::string {
    const auto encoded = path.generic_u8string();
    return {encoded.begin(), encoded.end()};
}

auto load_messages(const std::string& locale, const std::vector<std::string>& catalog_paths)
    -> vulpes::core::Localizer {
    vulpes::core::Localizer messages{locale};
    for (const auto& catalog_path : catalog_paths)
        messages.load_catalog_file(std::filesystem::path{catalog_path});
    return messages;
}

void browse(vulpes::db::Database& database, const vulpes::db::TableSchema& table,
            const vulpes::core::Localizer& messages) {
    vulpes::terminal::ConsoleTerminal terminal;
    vulpes::ui::BrowseDocument document{database, table, messages};
    vulpes::ui::DocumentSession{
        terminal, document, {20, 6}, messages.translate("terminal.minimum_size", {{"width", "20"}, {"height", "6"}})}
        .run();
}

void sql_console(vulpes::db::Database& database, const vulpes::core::Localizer& messages) {
    vulpes::terminal::ConsoleTerminal terminal;
    vulpes::ui::SqlDocument document{database, messages};
    vulpes::ui::DocumentSession{
        terminal, document, {20, 8}, messages.translate("terminal.minimum_size", {{"width", "20"}, {"height", "8"}})}
        .run();
}

auto run_terminal_diagnostics(const std::string& locale, const std::vector<std::string>& catalog_paths) -> int {
    auto messages = load_messages(locale, catalog_paths);

    vulpes::terminal::ConsoleTerminal terminal;
    vulpes::ui::TerminalDiagnostics document{messages};
    vulpes::ui::DocumentSession{
        terminal, document, {40, 10}, messages.translate("terminal.minimum_size", {{"width", "40"}, {"height", "10"}})}
        .run();
    return 0;
}

auto print_terminal_capabilities(const std::string& locale, const std::vector<std::string>& catalog_paths) -> int {
    const auto messages = load_messages(locale, catalog_paths);
    const auto capabilities = vulpes::terminal::detect_console_capabilities();
    const auto state = [&](bool connected) {
        return messages.translate(connected ? "terminal.capabilities.connected" : "terminal.capabilities.redirected");
    };

    std::cout << messages.translate("terminal.capabilities.title") << '\n'
              << messages.translate("terminal.capabilities.input") << ": "
              << state(capabilities.standard_input_is_terminal) << '\n'
              << messages.translate("terminal.capabilities.output") << ": "
              << state(capabilities.standard_output_is_terminal) << '\n'
              << messages.translate("terminal.capabilities.available") << ": "
              << messages.translate(capabilities.supports_interactive_terminal() ? "terminal.capabilities.yes"
                                                                                 : "terminal.capabilities.no")
              << '\n';
    return 0;
}

void print_schema(const vulpes::db::TableSchema& table, const vulpes::core::Localizer& messages) {
    std::cout << messages.translate("schema.title", {{"name", table.name}}) << '\n';
    for (const auto& field : table.fields) {
        std::cout << "  " << field.name;
        if (!field.declared_type.empty())
            std::cout << " : " << field.declared_type;
        if (!field.nullable)
            std::cout << " [" << messages.translate("schema.not_null") << ']';
        if (field.primary_key)
            std::cout << " [" << messages.translate("schema.primary_key") << ']';
        if (field.unique)
            std::cout << " [" << messages.translate("schema.unique") << ']';
        if (field.generated)
            std::cout << " [" << messages.translate("schema.generated") << ']';
        std::cout << '\n';
    }
}

auto run_workspace(const std::string& locale, const std::vector<std::string>& catalog_paths,
                   const vulpes::ui::Theme& theme, const std::filesystem::path& preferences_path) -> int {
    auto messages = load_messages(locale, catalog_paths);
    auto preferences = vulpes::core::WorkspacePreferences::load(preferences_path);
    vulpes::terminal::ConsoleTerminal terminal;
    auto size = terminal.size();
    if (size.width <= 0 || size.height <= 0)
        throw vulpes::Error{vulpes::ErrorCategory::terminal, "terminal reported an invalid size"};
    vulpes::terminal::ScreenBuffer previous{size.width, size.height};
    vulpes::terminal::ScreenBuffer current{size.width, size.height};
    vulpes::core::ActionMap actions;
    vulpes::ui::Workspace workspace{vulpes::ui::make_workspace_text(messages), theme};
    const auto refresh_recent_databases = [&] {
        std::vector<std::string> paths;
        paths.reserve(preferences.recent_databases().size());
        for (const auto& path : preferences.recent_databases())
            paths.push_back(path_text(path));
        workspace.set_recent_databases(std::move(paths));
    };
    refresh_recent_databases();
    std::optional<vulpes::db::Database> database;
    using WorkspaceSurface =
        std::variant<vulpes::ui::BrowseDocument, vulpes::ui::SchemaDocument, vulpes::ui::SqlDocument>;
    std::unordered_map<std::string, WorkspaceSurface> surfaces;
    const auto host_active_surface = [&](const vulpes::db::TableSchema* table = nullptr) {
        if (!database)
            return;
        const auto& document = workspace.active_document();
        switch (document.kind) {
        case vulpes::ui::DocumentKind::browse:
            if (table == nullptr)
                table = workspace.selected_table();
            if (table != nullptr) {
                surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::BrowseDocument>, *database, *table,
                                     messages);
            }
            return;
        case vulpes::ui::DocumentKind::schema:
            if (table != nullptr) {
                surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::SchemaDocument>, *table, messages);
            }
            return;
        case vulpes::ui::DocumentKind::sql_console:
            surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::SqlDocument>, *database, messages);
            return;
        case vulpes::ui::DocumentKind::workspace:
            return;
        }
    };
    for (;;) {
        const auto updated = terminal.size();
        if (updated.width <= 0 || updated.height <= 0)
            throw vulpes::Error{vulpes::ErrorCategory::terminal, "terminal reported an invalid size"};
        if (updated.width != size.width || updated.height != size.height) {
            size = updated;
            previous = vulpes::terminal::ScreenBuffer{size.width, size.height};
            current = vulpes::terminal::ScreenBuffer{size.width, size.height};
        }
        if (size.width < 40 || size.height < 10) {
            current.clear();
            vulpes::ui::render_terminal_warning(
                current, size, messages.translate("terminal.minimum_size", {{"width", "40"}, {"height", "10"}}));
            terminal.present(previous, current);
            previous = current;
            const auto action = actions.action_for(terminal.read_event());
            if (action == vulpes::core::ActionId::application_back ||
                action == vulpes::core::ActionId::application_quit)
                return 0;
            continue;
        }
        current.clear();
        const auto& active_document = workspace.active_document();
        if (active_document.kind != vulpes::ui::DocumentKind::workspace) {
            if (const auto surface = surfaces.find(active_document.id); surface != surfaces.end()) {
                std::visit([&](auto& document) { document.render(current, {0, 2, size.width, size.height - 3}); },
                           surface->second);
            }
        }
        workspace.render(current, {0, 0, size.width, size.height});
        terminal.present(previous, current);
        previous = current;
        const auto event = terminal.read_event();
        const auto action = actions.action_for(event);
        const auto active_document_id = workspace.active_document().id;
        const auto outcome = workspace.handle(action, event);
        if (outcome == vulpes::ui::WorkspaceResult::quit)
            return 0;
        if (outcome == vulpes::ui::WorkspaceResult::open_database ||
            outcome == vulpes::ui::WorkspaceResult::open_database_read_only ||
            outcome == vulpes::ui::WorkspaceResult::create_database) {
            try {
                const auto path = workspace.requested_path();
                if (path.empty())
                    throw vulpes::Error{vulpes::ErrorCategory::validation,
                                        messages.translate("workspace.path_required")};
                surfaces.clear();
                const std::filesystem::path database_path{path};
                const auto open_mode = outcome == vulpes::ui::WorkspaceResult::create_database
                                           ? vulpes::db::OpenMode::read_write_create
                                       : outcome == vulpes::ui::WorkspaceResult::open_database_read_only
                                           ? vulpes::db::OpenMode::read_only
                                           : vulpes::db::OpenMode::read_write;
                database.emplace(database_path, open_mode);
                workspace.set_database(path, vulpes::db::inspect_schema(*database),
                                       open_mode == vulpes::db::OpenMode::read_only);
                preferences.add_recent_database(database_path);
                preferences.save(preferences_path);
                refresh_recent_databases();
            } catch (const vulpes::Error& error) {
                workspace.set_status(error.what());
            }
        } else if (outcome == vulpes::ui::WorkspaceResult::command) {
            try {
                const auto command = vulpes::core::parse_command(workspace.requested_command());
                const auto show_invalid_arguments = [&] {
                    workspace.set_status(
                        messages.translate("error.invalid_command_arguments",
                                           {{"command", std::string{vulpes::core::action_id(command.id)}}}));
                };
                if (command.id == vulpes::core::CommandId::none) {
                    workspace.set_status(messages.translate("command.help"));
                } else if (!database) {
                    if (command.id == vulpes::core::CommandId::help) {
                        if (command.arguments.empty())
                            workspace.set_status(messages.translate("command.help"));
                        else
                            show_invalid_arguments();
                    } else if (command.id == vulpes::core::CommandId::quit) {
                        if (command.arguments.empty())
                            return 0;
                        show_invalid_arguments();
                    } else if (command.id == vulpes::core::CommandId::unknown) {
                        workspace.set_status(messages.translate("application.unknown_command"));
                    } else {
                        workspace.set_status(messages.translate("workspace.command_database_required"));
                    }
                } else {
                    vulpes::core::ApplicationRuntime application{*database};
                    const auto response = application.execute(command);
                    switch (response.outcome) {
                    case vulpes::core::CommandOutcome::help:
                        workspace.set_status(messages.translate("command.help"));
                        break;
                    case vulpes::core::CommandOutcome::tables:
                        workspace.set_tables(response.tables);
                        workspace.set_status(messages.translate("workspace.command_tables",
                                                                {{"count", std::to_string(response.tables.size())}}));
                        break;
                    case vulpes::core::CommandOutcome::schema:
                        workspace.open_schema(*response.table);
                        host_active_surface(&*response.table);
                        break;
                    case vulpes::core::CommandOutcome::browse:
                        workspace.open_browse(*response.table);
                        host_active_surface(&*response.table);
                        break;
                    case vulpes::core::CommandOutcome::sql:
                        workspace.open_sql_console();
                        host_active_surface();
                        break;
                    case vulpes::core::CommandOutcome::quit:
                        return 0;
                    case vulpes::core::CommandOutcome::unknown_command:
                        workspace.set_status(messages.translate("application.unknown_command"));
                        break;
                    case vulpes::core::CommandOutcome::invalid_arguments:
                        show_invalid_arguments();
                        break;
                    case vulpes::core::CommandOutcome::table_not_found:
                        workspace.set_status(
                            messages.translate("error.unknown_table", {{"name", command.arguments.front()}}));
                        break;
                    }
                }
            } catch (const vulpes::Error& error) {
                workspace.set_status(error.what());
            }
        } else if ((outcome == vulpes::ui::WorkspaceResult::browse_table && database && workspace.selected_table()) ||
                   (outcome == vulpes::ui::WorkspaceResult::run_sql && database)) {
            host_active_surface();
        } else if (outcome == vulpes::ui::WorkspaceResult::unchanged) {
            const auto& document = workspace.active_document();
            if (document.kind != vulpes::ui::DocumentKind::workspace) {
                if (const auto surface = surfaces.find(document.id); surface != surfaces.end()) {
                    const auto result = std::visit(
                        [&](auto& active_surface) { return active_surface.handle(action, event); }, surface->second);
                    if (result == vulpes::ui::DocumentResult::close) {
                        const auto closed_id = document.id;
                        static_cast<void>(workspace.close_active_document());
                        surfaces.erase(closed_id);
                    }
                }
            }
        }
        if (!workspace.has_document(active_document_id))
            surfaces.erase(active_document_id);
    }
}

auto run(const std::filesystem::path& database_path, const std::optional<std::string>& table_name,
         const std::optional<std::string>& command_source, const std::string& locale,
         const std::vector<std::string>& catalog_paths) -> int {
    vulpes::db::Database database{database_path, vulpes::db::OpenMode::read_write};
    auto messages = load_messages(locale, catalog_paths);
    vulpes::core::ApplicationRuntime application{database};
    std::cout << messages.translate("application.title") << " " << vulpes::build::version << "\n\n";

    vulpes::core::Command command{.id = vulpes::core::CommandId::tables};
    if (table_name)
        command = {.id = vulpes::core::CommandId::browse, .arguments = {*table_name}};
    else if (command_source)
        command = vulpes::core::parse_command(*command_source);

    const auto response = application.execute(command);
    switch (response.outcome) {
    case vulpes::core::CommandOutcome::help:
        std::cout << messages.translate("command.help") << '\n';
        break;
    case vulpes::core::CommandOutcome::tables:
        std::cout << messages.translate("database.tables") << ":\n";
        for (const auto& table : response.tables)
            std::cout << "  " << table.name << (table.is_view ? messages.translate("database.view_suffix") : "")
                      << '\n';
        break;
    case vulpes::core::CommandOutcome::schema:
        print_schema(*response.table, messages);
        break;
    case vulpes::core::CommandOutcome::browse:
        browse(database, *response.table, messages);
        break;
    case vulpes::core::CommandOutcome::sql:
        sql_console(database, messages);
        break;
    case vulpes::core::CommandOutcome::quit:
        break;
    case vulpes::core::CommandOutcome::unknown_command:
        std::cerr << messages.translate("application.unknown_command") << '\n';
        return 1;
    case vulpes::core::CommandOutcome::invalid_arguments:
        std::cerr << messages.translate("error.invalid_command_arguments",
                                        {{"command", std::string{vulpes::core::action_id(response.command)}}})
                  << '\n';
        return 1;
    case vulpes::core::CommandOutcome::table_not_found:
        std::cerr << messages.translate("error.unknown_table", {{"name", command.arguments.front()}}) << '\n';
        return 1;
    }
    return 0;
}

} // namespace

#ifdef _WIN32
auto wmain(int argc, wchar_t** argv) -> int {
#else
auto main(int argc, char** argv) -> int {
#endif
    initialize_console_encoding();
    try {
        CLI::App app{"A keyboard-first RAD environment for local SQLite applications"};
        bool version = false;
        std::string database_argument;
        std::string table_name;
        std::string command_source;
        bool sql = false;
        bool terminal_diagnostics = false;
        bool terminal_capabilities = false;
        std::string locale{"en"};
        std::string theme_name{"midnight"};
        std::string preferences_argument;
        std::vector<std::string> catalog_paths;
        app.add_flag("--version", version, "Show Vulpes version and exit");
        app.add_option("database", database_argument, "SQLite database path");
        const auto table_option = app.add_option("--table", table_name, "Browse one table or view");
        const auto command_option = app.add_option("--command", command_source, "Run one Vulpes command and exit");
        const auto sql_option = app.add_flag("--sql", sql, "Open the interactive SQL console");
        const auto terminal_diagnostics_option =
            app.add_flag("--terminal-diagnostics", terminal_diagnostics,
                         "Open interactive normalized terminal-input and resize diagnostics");
        const auto terminal_capabilities_option = app.add_flag("--terminal-capabilities", terminal_capabilities,
                                                               "Print terminal stream capabilities and exit");
        table_option->excludes(command_option);
        table_option->excludes(sql_option);
        command_option->excludes(sql_option);
        terminal_diagnostics_option->excludes(table_option);
        terminal_diagnostics_option->excludes(command_option);
        terminal_diagnostics_option->excludes(sql_option);
        terminal_diagnostics_option->excludes(terminal_capabilities_option);
        terminal_capabilities_option->excludes(table_option);
        terminal_capabilities_option->excludes(command_option);
        terminal_capabilities_option->excludes(sql_option);
        app.add_option("--locale", locale, "BCP-47 locale for user-interface messages");
        app.add_option("--catalog", catalog_paths, "UTF-8 JSON message catalog; may be repeated");
        app.add_option("--theme", theme_name, "Workspace theme: midnight or high-contrast");
        app.add_option("--config", preferences_argument,
                       "Workspace preferences JSON path (defaults to the user configuration directory)");
        app.set_help_flag("-h,--help", "Show this help and exit");
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& error) {
            return app.exit(error);
        }

        if (version) {
            std::cout << "Vulpes " << vulpes::build::version << '\n';
            return 0;
        }
        if (terminal_capabilities) {
            if (!database_argument.empty())
                throw vulpes::Error{vulpes::ErrorCategory::validation,
                                    "--terminal-capabilities does not accept a database path"};
            return print_terminal_capabilities(locale, catalog_paths);
        }
        if (terminal_diagnostics) {
            if (!database_argument.empty())
                throw vulpes::Error{vulpes::ErrorCategory::validation,
                                    "--terminal-diagnostics does not accept a database path"};
            return run_terminal_diagnostics(locale, catalog_paths);
        }
        if (database_argument.empty()) {
            const auto preferences_path = preferences_argument.empty()
                                              ? vulpes::core::default_workspace_preferences_path()
                                              : std::filesystem::path{preferences_argument};
            return run_workspace(locale, catalog_paths, vulpes::ui::theme(vulpes::ui::parse_theme(theme_name)),
                                 preferences_path);
        }

        return run(std::filesystem::path{database_argument},
                   table_name.empty() ? std::nullopt : std::optional{table_name},
                   sql                      ? std::optional{std::string{"sql"}}
                   : command_source.empty() ? std::nullopt
                                            : std::optional{command_source},
                   locale, catalog_paths);
    } catch (const vulpes::Error& error) {
        std::cerr << "vulpes: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "vulpes: unexpected error: " << error.what() << '\n';
        return 1;
    }
}
