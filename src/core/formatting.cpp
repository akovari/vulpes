#include "vulpes/core/formatting.hpp"

#include "vulpes/core/error.hpp"

#include <algorithm>
#include <cmath>
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

} // namespace vulpes::core
