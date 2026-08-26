#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/version.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void print_usage() {
    std::cout << "Usage: vulpes [--version] [database.db]\n";
}

auto run(const std::filesystem::path& database_path) -> int {
    vulpes::db::Database database{database_path};
    const auto schema = vulpes::db::inspect_schema(database);
    std::cout << "Vulpes " << VULPES_VERSION << "\n\nTables and views:\n";
    for (const auto& table : schema) {
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
    try {
#ifdef _WIN32
        if (argc == 2 && std::wstring_view{argv[1]} == L"--version") {
#else
        if (argc == 2 && std::string_view{argv[1]} == "--version") {
#endif
            std::cout << "Vulpes " << VULPES_VERSION << '\n';
            return 0;
        }
        if (argc != 2) {
            print_usage();
            return argc == 1 ? 0 : 2;
        }

        return run(std::filesystem::path{argv[1]});
    } catch (const vulpes::Error& error) {
        std::cerr << "vulpes: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "vulpes: unexpected error: " << error.what() << '\n';
        return 1;
    }
}
