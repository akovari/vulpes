#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace vulpes::core {

enum class DateTimeStyle { short_, medium, long_, full };

struct NumberDisplayOptions {
    std::optional<std::uint16_t> minimum_fraction_digits;
    std::optional<std::uint16_t> maximum_fraction_digits;
    bool grouping{true};
};

// Locale-aware display formatting. Parsing and SQLite storage deliberately
// remain separate policies: display formatting must never mutate stored values.
class LocaleFormatter {
  public:
    explicit LocaleFormatter(std::string locale = "en", std::string time_zone = "UTC");

    [[nodiscard]] auto locale() const noexcept -> std::string_view { return locale_; }
    [[nodiscard]] auto time_zone() const noexcept -> std::string_view { return time_zone_; }
    [[nodiscard]] auto number(std::int64_t value, NumberDisplayOptions options = {}) const -> std::string;
    [[nodiscard]] auto number(double value, NumberDisplayOptions options = {}) const -> std::string;
    [[nodiscard]] auto currency(double value, std::string_view iso_currency_code) const -> std::string;
    [[nodiscard]] auto date(std::chrono::system_clock::time_point value,
                            DateTimeStyle style = DateTimeStyle::medium) const -> std::string;
    [[nodiscard]] auto time(std::chrono::system_clock::time_point value,
                            DateTimeStyle style = DateTimeStyle::medium) const -> std::string;
    [[nodiscard]] auto date_time(std::chrono::system_clock::time_point value,
                                 DateTimeStyle date_style = DateTimeStyle::medium,
                                 DateTimeStyle time_style = DateTimeStyle::medium) const -> std::string;
    [[nodiscard]] auto iso_date(std::string_view value, DateTimeStyle style = DateTimeStyle::medium) const
        -> std::string;
    [[nodiscard]] auto iso_time(std::string_view value, DateTimeStyle style = DateTimeStyle::medium) const
        -> std::string;
    [[nodiscard]] auto rfc3339(std::string_view value, DateTimeStyle date_style = DateTimeStyle::medium,
                               DateTimeStyle time_style = DateTimeStyle::medium) const -> std::string;

  private:
    std::string locale_;
    std::string time_zone_;
};

[[nodiscard]] auto normalize_iso_date(std::string_view value) -> std::string;
[[nodiscard]] auto normalize_iso_time(std::string_view value) -> std::string;
[[nodiscard]] auto normalize_rfc3339(std::string_view value) -> std::string;

} // namespace vulpes::core
