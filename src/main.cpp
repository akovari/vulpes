#include <CLI/CLI.hpp>

#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/version.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

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
        std::cout << found->name << (found->is_view ? " [view]" : "") << ":\n";
        for (const auto& field : found->fields) std::cout << "  " << field.name << "  " << field.declared_type << '\n';
    } else {
        std::cout << messages.translate("database.tables") << ":\n";
        for (const auto& table : schema) std::cout << "  " << table.name << (table.is_view ? " [view]" : "") << '\n';
    }
    return 0;
}

} // namespace

#ifdef _WIN32
auto wmain(int argc, wchar_t** argv) -> int {
#else
auto main(int argc, char** argv) -> int {
#endif
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

        return run(std::filesystem::path{database_argument}, table_name.empty() ? std::nullopt : std::optional{table_name});
    } catch (const vulpes::Error& error) {
        std::cerr << "vulpes: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "vulpes: unexpected error: " << error.what() << '\n';
        return 1;
    }
}
