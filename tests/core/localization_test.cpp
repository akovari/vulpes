#include "vulpes/core/localization.hpp"

#include <catch2/catch_test_macros.hpp>

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
