#include "vulpes/appmeta/loader.hpp"
#include "vulpes/core/actions.hpp"
#include "vulpes/core/application.hpp"
#include "vulpes/core/clipboard.hpp"
#include "vulpes/core/command.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/core/workspace_preferences.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/report/export.hpp"
#include "vulpes/script/runtime.hpp"
#include "vulpes/terminal/capabilities.hpp"
#include "vulpes/terminal/console_terminal.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/browse_document.hpp"
#include "vulpes/ui/document_session.hpp"
#include "vulpes/ui/report_document.hpp"
#include "vulpes/ui/schema_document.hpp"
#include "vulpes/ui/screen_document.hpp"
#include "vulpes/ui/sql_document.hpp"
#include "vulpes/ui/terminal_diagnostics.hpp"
#include "vulpes/ui/terminal_warning.hpp"
#include "vulpes/ui/theme.hpp"
#include "vulpes/ui/workspace.hpp"
#include "vulpes/version.hpp"

#include <CLI/CLI.hpp>
#include <concepts>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
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
            const vulpes::core::Localizer& messages, const vulpes::ui::Theme& theme,
            const vulpes::appmeta::ApplicationMetadata* metadata = nullptr,
            std::optional<vulpes::appmeta::TableMetadata> table_override = std::nullopt,
            vulpes::model::DatasetLifecycle* lifecycle = nullptr, std::size_t page_size = 100,
            const vulpes::appmeta::ViewDefinition* view = nullptr) {
    vulpes::terminal::ConsoleTerminal terminal;
    vulpes::core::SystemClipboard clipboard;
    vulpes::ui::BrowseDocument document{database,
                                        table,
                                        messages,
                                        theme,
                                        &clipboard,
                                        metadata,
                                        std::move(table_override),
                                        lifecycle,
                                        page_size,
                                        view == nullptr ? std::vector<vulpes::model::SavedFilter>{} : view->filters,
                                        view == nullptr ? std::nullopt : view->default_filter};
    vulpes::ui::DocumentSession{
        terminal, document, {20, 6}, messages.translate("terminal.minimum_size", {{"width", "20"}, {"height", "6"}})}
        .run();
}

void show_report(vulpes::db::Database& database, const vulpes::appmeta::ReportDefinition& report,
                 const vulpes::core::Localizer& messages, const vulpes::ui::Theme& theme) {
    vulpes::terminal::ConsoleTerminal terminal;
    vulpes::ui::ReportDocument document{database, report, messages, theme};
    vulpes::ui::DocumentSession{
        terminal, document, {20, 6}, messages.translate("terminal.minimum_size", {{"width", "20"}, {"height", "6"}})}
        .run();
}

void sql_console(vulpes::db::Database& database, const vulpes::core::Localizer& messages,
                 const vulpes::ui::Theme& theme) {
    vulpes::terminal::ConsoleTerminal terminal;
    vulpes::core::SystemClipboard clipboard;
    vulpes::ui::SqlDocument document{database, messages, theme, &clipboard};
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
                   const vulpes::ui::Theme& theme, std::size_t dataset_page_size,
                   vulpes::core::WorkspacePreferences preferences, const std::filesystem::path& preferences_path,
                   std::optional<std::filesystem::path> initial_database = std::nullopt) -> int {
    auto messages = load_messages(locale, catalog_paths);
    vulpes::terminal::ConsoleTerminal terminal;
    auto size = terminal.size();
    if (size.width <= 0 || size.height <= 0)
        throw vulpes::Error{vulpes::ErrorCategory::terminal, "terminal reported an invalid size"};
    vulpes::terminal::ScreenBuffer previous{size.width, size.height};
    vulpes::terminal::ScreenBuffer current{size.width, size.height};
    vulpes::core::ActionMap actions;
    for (const auto& binding : preferences.key_bindings())
        actions.bind(binding);
    vulpes::core::SystemClipboard clipboard;
    vulpes::ui::Workspace workspace{vulpes::ui::make_workspace_text(messages), theme, &clipboard};
    const auto refresh_recent_databases = [&] {
        std::vector<std::string> paths;
        paths.reserve(preferences.recent_databases().size());
        for (const auto& path : preferences.recent_databases())
            paths.push_back(path_text(path));
        workspace.set_recent_databases(std::move(paths));
    };
    refresh_recent_databases();
    std::optional<vulpes::db::Database> database;
    std::optional<vulpes::appmeta::ApplicationDefinition> application_definition;
    std::optional<vulpes::script::Runtime> scripts;
    using WorkspaceSurface =
        std::variant<vulpes::ui::BrowseDocument, vulpes::ui::SchemaDocument, vulpes::ui::SqlDocument,
                     vulpes::ui::ReportDocument, vulpes::ui::ScreenDocument>;
    std::unordered_map<std::string, WorkspaceSurface> surfaces;
    const auto host_active_surface = [&](const vulpes::db::TableSchema* table = nullptr,
                                         std::optional<vulpes::appmeta::TableMetadata> table_override = std::nullopt,
                                         const vulpes::appmeta::ViewDefinition* view = nullptr,
                                         const vulpes::appmeta::ReportDefinition* report = nullptr,
                                         const vulpes::appmeta::ScreenDefinition* screen = nullptr) {
        if (!database)
            return;
        const auto& document = workspace.active_document();
        switch (document.kind) {
        case vulpes::ui::DocumentKind::browse:
            if (table == nullptr)
                table = workspace.selected_table();
            if (table != nullptr) {
                surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::BrowseDocument>, *database, *table,
                                     messages, theme, &clipboard,
                                     application_definition ? &application_definition->presentation : nullptr,
                                     std::move(table_override), scripts ? &*scripts : nullptr, dataset_page_size,
                                     view == nullptr ? std::vector<vulpes::model::SavedFilter>{} : view->filters,
                                     view == nullptr ? std::nullopt : view->default_filter);
            }
            return;
        case vulpes::ui::DocumentKind::schema:
            if (table != nullptr) {
                surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::SchemaDocument>, *table, messages,
                                     theme);
            }
            return;
        case vulpes::ui::DocumentKind::sql_console:
            surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::SqlDocument>, *database, messages, theme,
                                 &clipboard);
            return;
        case vulpes::ui::DocumentKind::report:
            if (report != nullptr) {
                surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::ReportDocument>, *database, *report,
                                     messages, theme);
            }
            return;
        case vulpes::ui::DocumentKind::application_screen:
            if (screen != nullptr)
                surfaces.try_emplace(document.id, std::in_place_type<vulpes::ui::ScreenDocument>, *screen, theme);
            return;
        case vulpes::ui::DocumentKind::workspace:
            return;
        }
    };
    const auto open_database = [&](const std::filesystem::path& database_path, vulpes::db::OpenMode open_mode) {
        vulpes::db::Database opened_database{database_path, open_mode};
        auto definition = vulpes::appmeta::load_application_definition(opened_database);
        std::optional<vulpes::script::Runtime> opened_scripts;
        if (!definition.empty()) {
            opened_scripts.emplace(definition.scripts);
            opened_scripts->on_open();
        }
        auto tables = vulpes::db::inspect_schema(opened_database);
        if (!definition.empty()) {
            std::erase_if(tables,
                          [](const auto& table) { return vulpes::appmeta::is_application_metadata_table(table.name); });
        }
        surfaces.clear();
        database.emplace(std::move(opened_database));
        application_definition = std::move(definition);
        scripts = std::move(opened_scripts);
        workspace.set_database(path_text(database_path), std::move(tables),
                               open_mode == vulpes::db::OpenMode::read_only);
        if (!application_definition->empty()) {
            auto title = path_text(database_path.stem());
            if (const auto configured_title = application_definition->setting("title"))
                title = *configured_title;
            workspace.set_application(std::move(title), application_definition->menus);
            if (const auto* screen = application_definition->default_screen(); screen != nullptr) {
                workspace.open_screen(screen->name, screen->label);
                host_active_surface(nullptr, std::nullopt, nullptr, nullptr, screen);
            }
        }
        preferences.add_recent_database(database_path);
        preferences.save(preferences_path);
        refresh_recent_databases();
    };
    if (initial_database)
        open_database(*initial_database, vulpes::db::OpenMode::read_write);
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
                workspace.set_active_document_dirty(
                    std::visit([](const auto& document) { return document.is_dirty(); }, surface->second));
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
        std::optional<std::string> document_command;
        if (outcome == vulpes::ui::WorkspaceResult::unchanged) {
            const auto& document = workspace.active_document();
            if (document.kind != vulpes::ui::DocumentKind::workspace) {
                if (const auto surface = surfaces.find(document.id); surface != surfaces.end()) {
                    const auto result = std::visit(
                        [&](auto& active_surface) {
                            const auto result = active_surface.handle(action, event);
                            using Surface = std::remove_cvref_t<decltype(active_surface)>;
                            if constexpr (std::same_as<Surface, vulpes::ui::ScreenDocument>) {
                                if (result == vulpes::ui::DocumentResult::command) {
                                    if (const auto command_name = active_surface.take_command(); command_name)
                                        document_command = "run \"" + *command_name + '"';
                                }
                            }
                            return result;
                        },
                        surface->second);
                    if (result == vulpes::ui::DocumentResult::close) {
                        const auto closed_id = document.id;
                        static_cast<void>(workspace.close_active_document());
                        surfaces.erase(closed_id);
                    }
                }
            }
        }
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
                const std::filesystem::path database_path{path};
                const auto open_mode = outcome == vulpes::ui::WorkspaceResult::create_database
                                           ? vulpes::db::OpenMode::read_write_create
                                       : outcome == vulpes::ui::WorkspaceResult::open_database_read_only
                                           ? vulpes::db::OpenMode::read_only
                                           : vulpes::db::OpenMode::read_write;
                open_database(database_path, open_mode);
            } catch (const vulpes::Error& error) {
                workspace.set_status(error.what());
            }
        } else if (outcome == vulpes::ui::WorkspaceResult::command || document_command) {
            try {
                const auto command =
                    vulpes::core::parse_command(document_command ? std::string_view{*document_command}
                                                                 : std::string_view{workspace.requested_command()});
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
                    vulpes::core::ApplicationRuntime application{
                        *database, application_definition ? &*application_definition : nullptr,
                        scripts ? &*scripts : nullptr};
                    const auto response = application.execute(command);
                    switch (response.outcome) {
                    case vulpes::core::CommandOutcome::help:
                        workspace.set_status(messages.translate("command.help"));
                        break;
                    case vulpes::core::CommandOutcome::tables:
                        workspace.set_tables(response.tables);
                        workspace.set_status(
                            messages.translate("workspace.command_tables",
                                               {{"count", static_cast<std::int64_t>(response.tables.size())}}));
                        break;
                    case vulpes::core::CommandOutcome::schema:
                        workspace.open_schema(*response.table);
                        host_active_surface(&*response.table);
                        break;
                    case vulpes::core::CommandOutcome::browse:
                        if (response.view) {
                            const auto title = response.view->label.value_or(response.view->name);
                            workspace.open_browse(*response.table, "view:" + response.view->name, title);
                            host_active_surface(&*response.table,
                                                response.form ? std::optional{response.form->presentation}
                                                              : std::nullopt,
                                                &*response.view);
                        } else if (response.form) {
                            const auto title = response.form->presentation.label.value_or(response.form->name);
                            workspace.open_browse(*response.table, "form:" + response.form->name, title);
                            host_active_surface(&*response.table, response.form->presentation);
                        } else {
                            workspace.open_browse(*response.table);
                            host_active_surface(&*response.table);
                        }
                        break;
                    case vulpes::core::CommandOutcome::sql:
                        workspace.open_sql_console();
                        host_active_surface();
                        break;
                    case vulpes::core::CommandOutcome::quit:
                        return 0;
                    case vulpes::core::CommandOutcome::forms:
                        workspace.set_status(messages.translate(
                            "workspace.command_forms", {{"count", static_cast<std::int64_t>(response.forms.size())}}));
                        break;
                    case vulpes::core::CommandOutcome::screens:
                        workspace.set_status(
                            messages.translate("workspace.command_screens",
                                               {{"count", static_cast<std::int64_t>(response.screens.size())}}));
                        break;
                    case vulpes::core::CommandOutcome::screen:
                        workspace.open_screen(response.screen->name, response.screen->label);
                        host_active_surface(nullptr, std::nullopt, nullptr, nullptr, &*response.screen);
                        break;
                    case vulpes::core::CommandOutcome::views:
                        workspace.set_status(messages.translate(
                            "workspace.command_views", {{"count", static_cast<std::int64_t>(response.views.size())}}));
                        break;
                    case vulpes::core::CommandOutcome::reports:
                        workspace.set_status(
                            messages.translate("workspace.command_reports",
                                               {{"count", static_cast<std::int64_t>(response.reports.size())}}));
                        break;
                    case vulpes::core::CommandOutcome::report:
                        workspace.open_report(response.report->name, response.report->label);
                        host_active_surface(nullptr, std::nullopt, nullptr, &*response.report);
                        break;
                    case vulpes::core::CommandOutcome::export_report: {
                        const auto summary =
                            vulpes::report::export_query(*database, response.report->sql, response.report->row_limit,
                                                         {.destination = response.export_destination,
                                                          .format = *response.export_format,
                                                          .overwrite = response.export_overwrite,
                                                          .title = response.report->label,
                                                          .locale = std::string{messages.locale()}});
                        workspace.set_status(messages.translate(
                            "export.complete",
                            {{"rows", static_cast<std::int64_t>(summary.rows)},
                             {"path", path_text(summary.destination)},
                             {"format", std::string{vulpes::report::export_format_name(summary.format)}}}));
                        break;
                    }
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
                    case vulpes::core::CommandOutcome::definition_not_found:
                        workspace.set_status(messages.translate(
                            "error.unknown_definition",
                            {{"name", command.arguments.empty() ? command.name : command.arguments.front()}}));
                        break;
                    case vulpes::core::CommandOutcome::command_cycle:
                        workspace.set_status(messages.translate("error.command_cycle"));
                        break;
                    }
                }
            } catch (const vulpes::Error& error) {
                workspace.set_status(error.what());
            }
        } else if ((outcome == vulpes::ui::WorkspaceResult::browse_table && database && workspace.selected_table()) ||
                   (outcome == vulpes::ui::WorkspaceResult::run_sql && database)) {
            host_active_surface();
        }
        if (!workspace.has_document(active_document_id))
            surfaces.erase(active_document_id);
    }
}

auto run(const std::filesystem::path& database_path, const std::optional<std::string>& table_name,
         const std::optional<std::string>& command_source, const std::string& locale,
         const std::vector<std::string>& catalog_paths, const vulpes::ui::Theme& theme, std::size_t dataset_page_size)
    -> int {
    vulpes::db::Database database{database_path, vulpes::db::OpenMode::read_write};
    auto messages = load_messages(locale, catalog_paths);
    const auto application_definition = vulpes::appmeta::load_application_definition(database);
    std::optional<vulpes::script::Runtime> scripts;
    if (!application_definition.empty()) {
        scripts.emplace(application_definition.scripts);
        scripts->on_open();
    }
    vulpes::core::ApplicationRuntime application{
        database, application_definition.empty() ? nullptr : &application_definition, scripts ? &*scripts : nullptr};
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
        browse(database, *response.table, messages, theme,
               application_definition.empty() ? nullptr : &application_definition.presentation,
               response.form ? std::optional{response.form->presentation} : std::nullopt, scripts ? &*scripts : nullptr,
               dataset_page_size, response.view ? &*response.view : nullptr);
        break;
    case vulpes::core::CommandOutcome::sql:
        sql_console(database, messages, theme);
        break;
    case vulpes::core::CommandOutcome::quit:
        break;
    case vulpes::core::CommandOutcome::forms:
        for (const auto& form : response.forms)
            std::cout << "  " << form.name << " -> " << form.table << '\n';
        break;
    case vulpes::core::CommandOutcome::screens:
        for (const auto& screen : response.screens)
            std::cout << "  " << screen.name << " - " << screen.label << '\n';
        break;
    case vulpes::core::CommandOutcome::screen:
        std::cout << response.screen->label << '\n';
        if (response.screen->description)
            std::cout << "  " << *response.screen->description << '\n';
        for (const auto& item : response.screen->items)
            std::cout << "  " << item.label << " -> " << item.command << '\n';
        break;
    case vulpes::core::CommandOutcome::views:
        for (const auto& view : response.views)
            std::cout << "  " << view.name << " -> " << view.table << '\n';
        break;
    case vulpes::core::CommandOutcome::reports:
        for (const auto& report : response.reports)
            std::cout << "  " << report.name << " - " << report.label << '\n';
        break;
    case vulpes::core::CommandOutcome::report:
        show_report(database, *response.report, messages, theme);
        break;
    case vulpes::core::CommandOutcome::export_report: {
        const auto summary = vulpes::report::export_query(database, response.report->sql, response.report->row_limit,
                                                          {.destination = response.export_destination,
                                                           .format = *response.export_format,
                                                           .overwrite = response.export_overwrite,
                                                           .title = response.report->label,
                                                           .locale = std::string{messages.locale()}});
        std::cout << messages.translate("export.complete",
                                        {{"rows", static_cast<std::int64_t>(summary.rows)},
                                         {"path", path_text(summary.destination)},
                                         {"format", std::string{vulpes::report::export_format_name(summary.format)}}})
                  << '\n';
        break;
    }
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
    case vulpes::core::CommandOutcome::definition_not_found:
        std::cerr << messages.translate(
                         "error.unknown_definition",
                         {{"name", command.arguments.empty() ? command.name : command.arguments.front()}})
                  << '\n';
        return 1;
    case vulpes::core::CommandOutcome::command_cycle:
        std::cerr << messages.translate("error.command_cycle") << '\n';
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
        std::string query_source;
        std::string export_output_argument;
        std::string export_format_argument;
        std::size_t export_row_limit{100'000};
        bool overwrite_export = false;
        bool sql = false;
        bool terminal_diagnostics = false;
        bool terminal_capabilities = false;
        bool migrate_app_metadata = false;
        std::string locale_argument;
        std::string theme_argument;
        std::size_t page_size_argument{};
        std::string preferences_argument;
        std::vector<std::string> catalog_paths;
        app.add_flag("--version", version, "Show Vulpes version and exit");
        app.add_option("database", database_argument, "SQLite database path");
        const auto table_option = app.add_option("--table", table_name, "Browse one table or view");
        const auto command_option = app.add_option("--command", command_source, "Run one Vulpes command and exit");
        const auto sql_option = app.add_flag("--sql", sql, "Open the interactive SQL console");
        const auto query_option = app.add_option("--query", query_source, "Export one read-only SQL query");
        const auto output_option = app.add_option("--output", export_output_argument, "Query export destination");
        const auto export_format_option =
            app.add_option("--format", export_format_argument, "Query export format; defaults to the output extension");
        const auto export_row_limit_option =
            app.add_option("--row-limit", export_row_limit, "Maximum query export rows")
                ->check(CLI::Range(std::size_t{1}, std::size_t{100'000}))
                ->capture_default_str();
        const auto overwrite_export_option =
            app.add_flag("--overwrite", overwrite_export, "Replace an existing query export destination");
        query_option->needs(output_option);
        output_option->needs(query_option);
        export_format_option->needs(query_option);
        export_row_limit_option->needs(query_option);
        overwrite_export_option->needs(query_option);
        const auto terminal_diagnostics_option =
            app.add_flag("--terminal-diagnostics", terminal_diagnostics,
                         "Open interactive normalized terminal-input and resize diagnostics");
        const auto terminal_capabilities_option = app.add_flag("--terminal-capabilities", terminal_capabilities,
                                                               "Print terminal stream capabilities and exit");
        const auto migrate_app_option = app.add_flag("--migrate-app", migrate_app_metadata,
                                                     "Create or upgrade Vulpes application metadata in the database");
        table_option->excludes(command_option);
        table_option->excludes(sql_option);
        table_option->excludes(query_option);
        command_option->excludes(sql_option);
        command_option->excludes(query_option);
        sql_option->excludes(query_option);
        terminal_diagnostics_option->excludes(table_option);
        terminal_diagnostics_option->excludes(command_option);
        terminal_diagnostics_option->excludes(sql_option);
        terminal_diagnostics_option->excludes(query_option);
        terminal_diagnostics_option->excludes(terminal_capabilities_option);
        terminal_capabilities_option->excludes(table_option);
        terminal_capabilities_option->excludes(command_option);
        terminal_capabilities_option->excludes(sql_option);
        terminal_capabilities_option->excludes(query_option);
        migrate_app_option->excludes(table_option);
        migrate_app_option->excludes(command_option);
        migrate_app_option->excludes(sql_option);
        migrate_app_option->excludes(query_option);
        migrate_app_option->excludes(terminal_diagnostics_option);
        migrate_app_option->excludes(terminal_capabilities_option);
        const auto locale_option =
            app.add_option("--locale", locale_argument, "BCP-47 locale for user-interface messages");
        app.add_option("--catalog", catalog_paths, "UTF-8 JSON message catalog; may be repeated");
        const auto theme_option =
            app.add_option("--theme", theme_argument, "Workspace theme: midnight or high-contrast");
        const auto page_size_option =
            app.add_option("--page-size", page_size_argument, "Rows to load per table dataset (1-1000)")
                ->check(CLI::Range(vulpes::core::WorkspacePreferences::minimum_dataset_page_size,
                                   vulpes::core::WorkspacePreferences::maximum_dataset_page_size));
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
        const auto preferences_path = preferences_argument.empty() ? vulpes::core::default_workspace_preferences_path()
                                                                   : std::filesystem::path{preferences_argument};
        const auto preferences = vulpes::core::WorkspacePreferences::load(preferences_path);
        const auto locale = locale_option->count() > 0U ? locale_argument : preferences.locale();
        const auto theme_name = theme_option->count() > 0U ? theme_argument : preferences.theme();
        const auto dataset_page_size =
            page_size_option->count() > 0U ? page_size_argument : preferences.default_dataset_page_size();
        const auto& theme = vulpes::ui::theme(vulpes::ui::parse_theme(theme_name));
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
        if (migrate_app_metadata) {
            if (database_argument.empty())
                throw vulpes::Error{vulpes::ErrorCategory::validation, "--migrate-app requires a database path"};
            vulpes::db::Database database{std::filesystem::path{database_argument}, vulpes::db::OpenMode::read_write};
            vulpes::appmeta::migrate_application_metadata(database);
            std::cout << "Vulpes application metadata schema " << *vulpes::appmeta::application_schema_version(database)
                      << '\n';
            return 0;
        }
        if (!query_source.empty()) {
            if (database_argument.empty())
                throw vulpes::Error{vulpes::ErrorCategory::validation, "--query requires a database path"};
            auto messages = load_messages(locale, catalog_paths);
            vulpes::db::Database database{std::filesystem::path{database_argument}, vulpes::db::OpenMode::read_only};
            const std::filesystem::path output_path{export_output_argument};
            const auto format = export_format_argument.empty()
                                    ? vulpes::report::infer_export_format(output_path)
                                    : vulpes::report::parse_export_format(export_format_argument);
            const auto summary = vulpes::report::export_query(database, query_source, export_row_limit,
                                                              {.destination = output_path,
                                                               .format = format,
                                                               .overwrite = overwrite_export,
                                                               .title = messages.translate("export.query_title"),
                                                               .locale = std::string{messages.locale()}});
            std::cout << messages.translate(
                             "export.complete",
                             {{"rows", static_cast<std::int64_t>(summary.rows)},
                              {"path", path_text(summary.destination)},
                              {"format", std::string{vulpes::report::export_format_name(summary.format)}}})
                      << '\n';
            return 0;
        }
        if (database_argument.empty()) {
            return run_workspace(locale, catalog_paths, theme, dataset_page_size, preferences, preferences_path);
        }

        if (table_name.empty() && command_source.empty() && !sql) {
            return run_workspace(locale, catalog_paths, theme, dataset_page_size, preferences, preferences_path,
                                 std::filesystem::path{database_argument});
        }

        return run(std::filesystem::path{database_argument},
                   table_name.empty() ? std::nullopt : std::optional{table_name},
                   sql                      ? std::optional{std::string{"sql"}}
                   : command_source.empty() ? std::nullopt
                                            : std::optional{command_source},
                   locale, catalog_paths, theme, dataset_page_size);
    } catch (const vulpes::Error& error) {
        std::cerr << "vulpes: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "vulpes: unexpected error: " << error.what() << '\n';
        return 1;
    }
}
