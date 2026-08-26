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
