#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

TEST_CASE("localizer selects locale catalogs and falls back to English", "[core][i18n]") {
    vulpes::core::Localizer localizer{"cs-CZ"};
    localizer.add_catalog("cs", {
                                    {"application.title", "Vulpes"},
                                    {"welcome", "Vítejte, {name}!"},
                                });

    CHECK(localizer.translate("welcome", {{"name", "Adam"}}) == "Vítejte, Adam!");
    CHECK(localizer.translate("database.tables") == "Tables and views");
    CHECK(localizer.translate("missing.key") == "missing.key");
}

TEST_CASE("localizer loads validated UTF-8 JSON catalogs", "[core][i18n]") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("vulpes-localization-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    {
        std::ofstream output{path, std::ios::binary};
        output << R"({"locale":"cs","messages":{"application.title":"Vulpes CZ","welcome":"Vitejte, {name}!"}})";
    }

    vulpes::core::Localizer localizer{"cs-CZ"};
    localizer.load_catalog_file(path);
    std::error_code error;
    std::filesystem::remove(path, error);

    CHECK(localizer.translate("application.title") == "Vulpes CZ");
    CHECK(localizer.translate("welcome", {{"name", "Adam"}}) == "Vitejte, Adam!");
    CHECK(localizer.translate("database.tables") == "Tables and views");
}

TEST_CASE("localizer rejects malformed catalog documents", "[core][i18n]") {
    const auto path = std::filesystem::temp_directory_path() /
                      ("vulpes-invalid-localization-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    {
        std::ofstream output{path, std::ios::binary};
        output << R"({"locale":false,"messages":[]})";
    }

    vulpes::core::Localizer localizer;
    CHECK_THROWS_AS(localizer.load_catalog_file(path), vulpes::Error);
    std::error_code error;
    std::filesystem::remove(path, error);
}

TEST_CASE("localizer applies translated ICU plural and select rules", "[core][i18n][message-format]") {
    vulpes::core::Localizer english{"en-US"};
    CHECK(english.translate("workspace.command_tables", {{"count", 1}}) == "Refreshed 1 table or view.");
    CHECK(english.translate("workspace.command_tables", {{"count", 2}}) == "Refreshed 2 tables and views.");
    CHECK(english.translate("workspace.access_mode", {{"mode", "read_only"}}) == " [read-only]");
    CHECK(english.translate("workspace.access_mode", {{"mode", "read_write"}}).empty());

    vulpes::core::Localizer czech{"cs-CZ"};
    czech.load_catalog_file(std::filesystem::path{VULPES_SOURCE_DIR} / "translations" / "cs.json");
    CHECK(czech.translate("workspace.command_tables", {{"count", 1}}) == "Aktualizována 1 tabulka nebo pohled.");
    CHECK(czech.translate("workspace.command_tables", {{"count", 3}}) == "Aktualizovány 3 tabulky nebo pohledy.");
    CHECK(czech.translate("workspace.command_tables", {{"count", 5}}) == "Aktualizováno 5 tabulek a pohledů.");
    CHECK(czech.translate("workspace.access_mode", {{"mode", "read_only"}}) == " [jen pro čtení]");
}

TEST_CASE("localizer preserves the fallback message grammar locale", "[core][i18n][fallback]") {
    vulpes::core::Localizer czech_without_catalog{"cs-CZ"};
    CHECK(czech_without_catalog.translate("workspace.command_tables", {{"count", 3}}) ==
          "Refreshed 3 tables and views.");
}

TEST_CASE("localizer canonicalizes case-insensitive BCP-47 catalog tags", "[core][i18n][locale]") {
    vulpes::core::Localizer localizer{"CS-cz"};
    localizer.add_catalog("CS", {{"welcome", "Vítejte"}});

    CHECK(localizer.locale() == "cs-CZ");
    CHECK(localizer.translate("welcome") == "Vítejte");
}

TEST_CASE("localizer rejects invalid ICU message patterns when catalogs are added", "[core][i18n][validation]") {
    vulpes::core::Localizer localizer;
    CHECK_THROWS_AS(localizer.add_catalog("cs", {{"broken", "{count, plural, one {one}"}}), vulpes::Error);
}
