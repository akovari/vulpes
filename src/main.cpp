#include "vulpes/core/browse_controller.hpp"
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

auto run(const std::filesystem::path& database_path, const std::optional<std::string>& table_name) -> int {
    vulpes::db::Database database{database_path};
    const auto schema = vulpes::db::inspect_schema(database);
    vulpes::core::Localizer messages;
    std::cout << messages.translate("application.title") << " " << VULPES_VERSION << "\n\n";
    if (table_name) {
        const auto found = std::ranges::find(schema, *table_name, &vulpes::db::TableSchema::name);
        if (found == schema.end()) {
            throw vulpes::Error{vulpes::ErrorCategory::validation,
                                messages.translate("error.unknown_table", {{"name", *table_name}})};
        }
        browse(database, *found);
    } else {
        std::cout << messages.translate("database.tables") << ":\n";
        for (const auto& table : schema)
            std::cout << "  " << table.name << (table.is_view ? " [view]" : "") << '\n';
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
        app.add_flag("--version", version, "Show Vulpes version and exit");
        app.add_option("database", database_argument, "SQLite database path");
        app.add_option("--table", table_name, "Inspect one table or view");
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
                   table_name.empty() ? std::nullopt : std::optional{table_name});
    } catch (const vulpes::Error& error) {
        std::cerr << "vulpes: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "vulpes: unexpected error: " << error.what() << '\n';
        return 1;
    }
}
