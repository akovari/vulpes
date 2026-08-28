#include "vulpes/core/error.hpp"
#include "vulpes/core/formatting.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>

namespace {

[[nodiscard]] auto sample_time() -> std::chrono::system_clock::time_point {
    using namespace std::chrono;
    return sys_days{year{2024} / January / 2} + hours{15} + minutes{4} + seconds{5};
}

} // namespace

TEST_CASE("locale formatter applies CLDR number and currency conventions", "[core][i18n][format]") {
    const vulpes::core::NumberDisplayOptions two_decimals{.minimum_fraction_digits = 2, .maximum_fraction_digits = 2};
    const vulpes::core::LocaleFormatter english{"en-US"};
    const vulpes::core::LocaleFormatter czech{"cs-CZ"};

    CHECK(english.number(12'345.67, two_decimals) == "12,345.67");
    CHECK(czech.number(12'345.67, two_decimals) == "12\u00a0345,67");
    CHECK(english.currency(1'234.5, "USD") == "$1,234.50");
    CHECK(czech.currency(1'234.5, "CZK") == "1\u00a0234,50\u00a0Kč");
}

TEST_CASE("locale formatter applies explicit date time style and zone policies", "[core][i18n][format]") {
    const vulpes::core::LocaleFormatter english{"en-US", "UTC"};
    const vulpes::core::LocaleFormatter czech{"cs-CZ", "Europe/Prague"};

    CHECK(english.date(sample_time(), vulpes::core::DateTimeStyle::short_) == "1/2/24");
    CHECK(czech.date(sample_time(), vulpes::core::DateTimeStyle::short_) == "02.01.24");
    CHECK(english.time(sample_time(), vulpes::core::DateTimeStyle::short_).find("3:04") != std::string::npos);
    CHECK(czech.time(sample_time(), vulpes::core::DateTimeStyle::short_) == "16:04");
    CHECK(english.date_time(sample_time(), vulpes::core::DateTimeStyle::short_, vulpes::core::DateTimeStyle::short_)
              .find("1/2/24") != std::string::npos);
}

TEST_CASE("locale formatter rejects ambiguous or invalid display policy", "[core][i18n][format]") {
    CHECK_THROWS_AS(vulpes::core::LocaleFormatter{"not_a_locale"}, vulpes::Error);
    CHECK_THROWS_AS(vulpes::core::LocaleFormatter("en", "Not/AZone"), vulpes::Error);
    const vulpes::core::LocaleFormatter formatter{"en"};
    CHECK_THROWS_AS(formatter.currency(1, "usd"), vulpes::Error);
    CHECK_THROWS_AS(formatter.number(1.0, {.minimum_fraction_digits = 3, .maximum_fraction_digits = 2}), vulpes::Error);
}

TEST_CASE("temporal formatting validates explicit SQLite text encodings", "[core][i18n][format][temporal]") {
    const vulpes::core::LocaleFormatter czech{"cs-CZ", "Europe/Prague"};

    CHECK(vulpes::core::normalize_iso_date("2024-02-29") == "2024-02-29");
    CHECK(vulpes::core::normalize_iso_time("09:07") == "09:07");
    CHECK(vulpes::core::normalize_iso_time("09:07:05") == "09:07:05");
    CHECK(vulpes::core::normalize_rfc3339("2024-01-02T16:04:05+01:00") == "2024-01-02T15:04:05Z");
    CHECK(czech.iso_date("2024-01-02", vulpes::core::DateTimeStyle::short_) == "02.01.24");
    CHECK(czech.iso_time("15:04:05", vulpes::core::DateTimeStyle::short_) == "15:04");
    CHECK(
        czech.rfc3339("2024-01-02T15:04:05Z", vulpes::core::DateTimeStyle::short_, vulpes::core::DateTimeStyle::short_)
            .find("16:04") != std::string::npos);

    CHECK_THROWS_AS(vulpes::core::normalize_iso_date("2023-02-29"), vulpes::Error);
    CHECK_THROWS_AS(vulpes::core::normalize_iso_time("24:00"), vulpes::Error);
    CHECK_THROWS_AS(vulpes::core::normalize_rfc3339("2024-01-02T15:04:05"), vulpes::Error);
}
