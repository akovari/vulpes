#include "vulpes/core/formatting.hpp"

#include "vulpes/core/error.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <memory>
#include <type_traits>
#include <unicode/currunit.h>
#include <unicode/datefmt.h>
#include <unicode/locid.h>
#include <unicode/numberformatter.h>
#include <unicode/stringpiece.h>
#include <unicode/timezone.h>
#include <unicode/utypes.h>
#include <utility>

namespace vulpes::core {
namespace {

struct ParsedDateTime {
    std::chrono::system_clock::time_point value;
    int year{};
    unsigned month{};
    unsigned day{};
    unsigned hour{};
    unsigned minute{};
    unsigned second{};
};

[[nodiscard]] auto integer_at(std::string_view text, std::size_t offset, std::size_t length,
                              std::string_view description) -> int {
    if (offset + length > text.size())
        throw Error{ErrorCategory::validation, "invalid " + std::string{description}};
    int result{};
    const auto* first = text.data() + offset;
    const auto* last = first + length;
    const auto [end, error] = std::from_chars(first, last, result);
    if (error != std::errc{} || end != last)
        throw Error{ErrorCategory::validation, "invalid " + std::string{description}};
    return result;
}

[[nodiscard]] auto parse_date(std::string_view text) -> ParsedDateTime {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-')
        throw Error{ErrorCategory::validation, "date must use YYYY-MM-DD"};
    const auto year = integer_at(text, 0, 4, "date");
    const auto month = static_cast<unsigned>(integer_at(text, 5, 2, "date"));
    const auto day = static_cast<unsigned>(integer_at(text, 8, 2, "date"));
    const std::chrono::year_month_day calendar{std::chrono::year{year}, std::chrono::month{month},
                                               std::chrono::day{day}};
    if (!calendar.ok())
        throw Error{ErrorCategory::validation, "date is outside the calendar: " + std::string{text}};
    return {.value = std::chrono::sys_days{calendar}, .year = year, .month = month, .day = day};
}

[[nodiscard]] auto parse_time(std::string_view text) -> ParsedDateTime {
    if ((text.size() != 5 && text.size() != 8) || text[2] != ':' || (text.size() == 8 && text[5] != ':'))
        throw Error{ErrorCategory::validation, "time must use HH:MM or HH:MM:SS"};
    const auto hour = static_cast<unsigned>(integer_at(text, 0, 2, "time"));
    const auto minute = static_cast<unsigned>(integer_at(text, 3, 2, "time"));
    const auto second = text.size() == 8 ? static_cast<unsigned>(integer_at(text, 6, 2, "time")) : 0U;
    if (hour > 23 || minute > 59 || second > 59)
        throw Error{ErrorCategory::validation, "time is outside the 24-hour clock: " + std::string{text}};
    const auto value = std::chrono::system_clock::time_point{} + std::chrono::hours{hour} +
                       std::chrono::minutes{minute} + std::chrono::seconds{second};
    return {.value = value, .hour = hour, .minute = minute, .second = second};
}

[[nodiscard]] auto parse_rfc3339(std::string_view text) -> ParsedDateTime {
    const bool utc = text.size() == 20 && text[19] == 'Z';
    const bool offset = text.size() == 25 && (text[19] == '+' || text[19] == '-') && text[22] == ':';
    if ((!utc && !offset) || text[10] != 'T')
        throw Error{ErrorCategory::validation,
                    "date-time must use YYYY-MM-DDTHH:MM:SSZ or an explicit +HH:MM/-HH:MM offset"};
    auto date = parse_date(text.substr(0, 10));
    const auto time = parse_time(text.substr(11, 8));
    auto value = date.value + std::chrono::hours{time.hour} + std::chrono::minutes{time.minute} +
                 std::chrono::seconds{time.second};
    if (offset) {
        const auto offset_hour = integer_at(text, 20, 2, "date-time offset");
        const auto offset_minute = integer_at(text, 23, 2, "date-time offset");
        if (offset_hour > 23 || offset_minute > 59)
            throw Error{ErrorCategory::validation, "date-time offset is outside the valid range"};
        const auto displacement = std::chrono::hours{offset_hour} + std::chrono::minutes{offset_minute};
        value += text[19] == '+' ? -displacement : displacement;
    }
    date.value = value;
    date.hour = time.hour;
    date.minute = time.minute;
    date.second = time.second;
    return date;
}

[[nodiscard]] auto utf8(std::string_view text) -> icu::UnicodeString {
    return icu::UnicodeString::fromUTF8(icu::StringPiece{text.data(), static_cast<std::int32_t>(text.size())});
}

[[nodiscard]] auto to_utf8(const icu::UnicodeString& text) -> std::string {
    std::string result;
    text.toUTF8String(result);
    return result;
}

[[noreturn]] void throw_icu(std::string_view operation, UErrorCode status) {
    throw Error{ErrorCategory::validation, std::string{operation} + ": " + u_errorName(status),
                static_cast<int>(status)};
}

[[nodiscard]] auto locale_for(std::string_view name) -> icu::Locale {
    UErrorCode status = U_ZERO_ERROR;
    auto locale =
        icu::Locale::forLanguageTag(icu::StringPiece{name.data(), static_cast<std::int32_t>(name.size())}, status);
    if (U_FAILURE(status) || locale.isBogus())
        throw_icu("invalid BCP-47 locale '" + std::string{name} + "'", status);
    return locale;
}

[[nodiscard]] auto canonical_locale(std::string_view name) -> std::string {
    UErrorCode status = U_ZERO_ERROR;
    const auto locale = locale_for(name);
    auto tag = locale.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || tag.empty())
        throw_icu("unable to canonicalize BCP-47 locale '" + std::string{name} + "'", status);
    return tag;
}

[[nodiscard]] auto canonical_time_zone(std::string_view name) -> std::string {
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString canonical;
    UBool is_system_id = false;
    icu::TimeZone::getCanonicalID(utf8(name), canonical, is_system_id, status);
    if (U_FAILURE(status) || !is_system_id)
        throw Error{ErrorCategory::validation, "unknown IANA time zone '" + std::string{name} + "'"};
    return to_utf8(canonical);
}

void validate_number_options(NumberDisplayOptions options) {
    constexpr std::uint16_t maximum_digits = 999;
    if ((options.minimum_fraction_digits && *options.minimum_fraction_digits > maximum_digits) ||
        (options.maximum_fraction_digits && *options.maximum_fraction_digits > maximum_digits) ||
        (options.minimum_fraction_digits && options.maximum_fraction_digits &&
         *options.minimum_fraction_digits > *options.maximum_fraction_digits)) {
        throw Error{ErrorCategory::validation, "invalid number fraction-digit range"};
    }
}

[[nodiscard]] auto precision_for(NumberDisplayOptions options) -> std::optional<icu::number::Precision> {
    if (!options.minimum_fraction_digits && !options.maximum_fraction_digits)
        return std::nullopt;
    const auto minimum = options.minimum_fraction_digits.value_or(0);
    const auto maximum = options.maximum_fraction_digits.value_or(999);
    return icu::number::Precision::minMaxFraction(minimum, maximum);
}

template <typename Value>
[[nodiscard]] auto format_number(Value value, const icu::Locale& locale, NumberDisplayOptions options) -> std::string {
    validate_number_options(options);
    auto formatter = icu::number::NumberFormatter::withLocale(locale).grouping(options.grouping ? UNUM_GROUPING_AUTO
                                                                                                : UNUM_GROUPING_OFF);
    if (const auto precision = precision_for(options))
        formatter = formatter.precision(*precision);

    UErrorCode status = U_ZERO_ERROR;
    auto formatted = [&] {
        if constexpr (std::is_same_v<Value, std::int64_t>)
            return formatter.formatInt(value, status);
        else
            return formatter.formatDouble(value, status);
    }();
    const auto result = formatted.toString(status);
    if (U_FAILURE(status))
        throw_icu("unable to format number", status);
    return to_utf8(result);
}

[[nodiscard]] auto icu_style(DateTimeStyle style) -> icu::DateFormat::EStyle {
    switch (style) {
    case DateTimeStyle::short_:
        return icu::DateFormat::kShort;
    case DateTimeStyle::medium:
        return icu::DateFormat::kMedium;
    case DateTimeStyle::long_:
        return icu::DateFormat::kLong;
    case DateTimeStyle::full:
        return icu::DateFormat::kFull;
    }
    return icu::DateFormat::kMedium;
}

[[nodiscard]] auto format_date(std::unique_ptr<icu::DateFormat> formatter, std::chrono::system_clock::time_point value,
                               std::string_view time_zone) -> std::string {
    if (!formatter)
        throw Error{ErrorCategory::validation, "unable to create ICU date/time formatter"};
    auto zone = std::unique_ptr<icu::TimeZone>{icu::TimeZone::createTimeZone(utf8(time_zone))};
    formatter->setTimeZone(*zone);
    const auto milliseconds = std::chrono::duration<double, std::milli>{value.time_since_epoch()}.count();
    icu::UnicodeString result;
    formatter->format(milliseconds, result);
    return to_utf8(result);
}

} // namespace

LocaleFormatter::LocaleFormatter(std::string locale, std::string time_zone)
    : locale_{canonical_locale(locale)}, time_zone_{canonical_time_zone(time_zone)} {
}

auto LocaleFormatter::number(std::int64_t value, NumberDisplayOptions options) const -> std::string {
    return format_number(value, locale_for(locale_), options);
}

auto LocaleFormatter::number(double value, NumberDisplayOptions options) const -> std::string {
    if (!std::isfinite(value))
        throw Error{ErrorCategory::validation, "non-finite numbers cannot be displayed"};
    return format_number(value, locale_for(locale_), options);
}

auto LocaleFormatter::currency(double value, std::string_view iso_currency_code) const -> std::string {
    if (!std::isfinite(value))
        throw Error{ErrorCategory::validation, "non-finite currency values cannot be displayed"};
    if (iso_currency_code.size() != 3 || !std::ranges::all_of(iso_currency_code, [](unsigned char character) {
            return character >= static_cast<unsigned char>('A') && character <= static_cast<unsigned char>('Z');
        })) {
        throw Error{ErrorCategory::validation, "currency display requires a three-letter uppercase ISO 4217 code"};
    }

    UErrorCode status = U_ZERO_ERROR;
    const icu::CurrencyUnit unit{
        icu::StringPiece{iso_currency_code.data(), static_cast<std::int32_t>(iso_currency_code.size())}, status};
    auto formatted =
        icu::number::NumberFormatter::withLocale(locale_for(locale_)).unit(unit).formatDouble(value, status);
    const auto result = formatted.toString(status);
    if (U_FAILURE(status))
        throw_icu("unable to format currency", status);
    return to_utf8(result);
}

auto LocaleFormatter::date(std::chrono::system_clock::time_point value, DateTimeStyle style) const -> std::string {
    return format_date(
        std::unique_ptr<icu::DateFormat>{icu::DateFormat::createDateInstance(icu_style(style), locale_for(locale_))},
        value, time_zone_);
}

auto LocaleFormatter::time(std::chrono::system_clock::time_point value, DateTimeStyle style) const -> std::string {
    return format_date(
        std::unique_ptr<icu::DateFormat>{icu::DateFormat::createTimeInstance(icu_style(style), locale_for(locale_))},
        value, time_zone_);
}

auto LocaleFormatter::date_time(std::chrono::system_clock::time_point value, DateTimeStyle date_style,
                                DateTimeStyle time_style) const -> std::string {
    return format_date(std::unique_ptr<icu::DateFormat>{icu::DateFormat::createDateTimeInstance(
                           icu_style(date_style), icu_style(time_style), locale_for(locale_))},
                       value, time_zone_);
}

auto LocaleFormatter::iso_date(std::string_view value, DateTimeStyle style) const -> std::string {
    return format_date(
        std::unique_ptr<icu::DateFormat>{icu::DateFormat::createDateInstance(icu_style(style), locale_for(locale_))},
        parse_date(value).value, "UTC");
}

auto LocaleFormatter::iso_time(std::string_view value, DateTimeStyle style) const -> std::string {
    return format_date(
        std::unique_ptr<icu::DateFormat>{icu::DateFormat::createTimeInstance(icu_style(style), locale_for(locale_))},
        parse_time(value).value, "UTC");
}

auto LocaleFormatter::rfc3339(std::string_view value, DateTimeStyle date_style, DateTimeStyle time_style) const
    -> std::string {
    return date_time(parse_rfc3339(value).value, date_style, time_style);
}

auto normalize_iso_date(std::string_view value) -> std::string {
    static_cast<void>(parse_date(value));
    return std::string{value};
}

auto normalize_iso_time(std::string_view value) -> std::string {
    const auto parsed = parse_time(value);
    return parsed.second == 0 && value.size() == 5
               ? std::format("{:02}:{:02}", parsed.hour, parsed.minute)
               : std::format("{:02}:{:02}:{:02}", parsed.hour, parsed.minute, parsed.second);
}

auto normalize_rfc3339(std::string_view value) -> std::string {
    const auto parsed = parse_rfc3339(value);
    const auto day = std::chrono::floor<std::chrono::days>(parsed.value);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{parsed.value - day};
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z", static_cast<int>(date.year()),
                       static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()), time.hours().count(),
                       time.minutes().count(), time.seconds().count());
}

} // namespace vulpes::core
