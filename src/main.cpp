#include "vulpes/core/application.hpp"
#include "vulpes/core/browse_controller.hpp"
#include "vulpes/core/command.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/console_terminal.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/grid.hpp"
#include "vulpes/version.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

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

void browse(vulpes::db::Database& database, const vulpes::db::TableSchema& table) {
    vulpes::model::Dataset dataset{database, table};
    vulpes::core::BrowseController controller{dataset};
    vulpes::terminal::ConsoleTerminal terminal;
    auto terminal_size = terminal.size();
    if (terminal_size.width < 20 || terminal_size.height < 6) {
        throw vulpes::Error{vulpes::ErrorCategory::terminal, "terminal must be at least 20 columns by 6 rows"};
    }
    vulpes::terminal::ScreenBuffer previous{terminal_size.width, terminal_size.height};
    vulpes::terminal::ScreenBuffer current{terminal_size.width, terminal_size.height};
    vulpes::ui::Grid grid{dataset, table.name};

    for (;;) {
        const auto updated_size = terminal.size();
        if (updated_size.width != terminal_size.width || updated_size.height != terminal_size.height) {
            terminal_size = updated_size;
            if (terminal_size.width < 20 || terminal_size.height < 6)
                continue;
            previous = vulpes::terminal::ScreenBuffer{terminal_size.width, terminal_size.height};
            current = vulpes::terminal::ScreenBuffer{terminal_size.width, terminal_size.height};
        }
        current.clear();
        grid.render(current, {0, 0, terminal_size.width, terminal_size.height});
        terminal.present(previous, current);
        previous = current;
        if (controller.handle(terminal.read_event()) == vulpes::core::BrowseResult::close)
            return;
    }
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

auto run(const std::filesystem::path& database_path, const std::optional<std::string>& table_name,
         const std::optional<std::string>& command_source) -> int {
    vulpes::db::Database database{database_path};
    vulpes::core::Localizer messages;
    vulpes::core::ApplicationRuntime application{database};
    std::cout << messages.translate("application.title") << " " << VULPES_VERSION << "\n\n";

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
            std::cout << "  " << table.name << (table.is_view ? " [view]" : "") << '\n';
        break;
    case vulpes::core::CommandOutcome::schema:
        print_schema(*response.table, messages);
        break;
    case vulpes::core::CommandOutcome::browse:
        browse(database, *response.table);
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
        app.add_flag("--version", version, "Show Vulpes version and exit");
        app.add_option("database", database_argument, "SQLite database path");
        const auto table_option = app.add_option("--table", table_name, "Browse one table or view");
        const auto command_option = app.add_option("--command", command_source, "Run one Vulpes command and exit");
        table_option->excludes(command_option);
        app.set_help_flag("-h,--help", "Show this help and exit");
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& error) {
            return app.exit(error);
        }

        if (version) {
            std::cout << "Vulpes " << VULPES_VERSION << '\n';
            return 0;
        }
        if (database_argument.empty()) {
            std::cout << app.help();
            return 0;
        }

        return run(std::filesystem::path{database_argument},
                   table_name.empty() ? std::nullopt : std::optional{table_name},
                   command_source.empty() ? std::nullopt : std::optional{command_source});
    } catch (const vulpes::Error& error) {
        std::cerr << "vulpes: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "vulpes: unexpected error: " << error.what() << '\n';
        return 1;
    }
}
