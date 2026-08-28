#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace vulpes::core {

class MessageValue {
  public:
    using Storage = std::variant<std::string, std::int64_t, double, bool>;

    MessageValue(const char* value) : storage_{std::string{value}} {}
    MessageValue(std::string value) : storage_{std::move(value)} {}
    MessageValue(std::string_view value) : storage_{std::string{value}} {}
    MessageValue(int value) noexcept : storage_{static_cast<std::int64_t>(value)} {}
    MessageValue(std::int64_t value) noexcept : storage_{value} {}
    MessageValue(double value) noexcept : storage_{value} {}
    MessageValue(bool value) noexcept : storage_{value} {}

    [[nodiscard]] auto storage() const noexcept -> const Storage& { return storage_; }

  private:
    Storage storage_;
};

using MessageArguments = std::map<std::string, MessageValue, std::less<>>;
using MessageCatalog = std::unordered_map<std::string, std::string>;

class LocalizedMessage {
  public:
    LocalizedMessage() = default;
    [[nodiscard]] auto format(const MessageArguments& arguments = {}) const -> std::string;

  private:
    friend class Localizer;
    LocalizedMessage(std::string pattern, std::string locale)
        : pattern_{std::move(pattern)}, locale_{std::move(locale)} {}

    std::string pattern_;
    std::string locale_{"en"};
};

class Localizer {
  public:
    explicit Localizer(std::string locale = "en");
    void add_catalog(std::string locale, MessageCatalog catalog);
    void load_catalog_file(const std::filesystem::path& path);
    [[nodiscard]] auto locale() const noexcept -> std::string_view { return locale_; }
    [[nodiscard]] auto bind(std::string_view key) const -> LocalizedMessage;
    [[nodiscard]] auto translate(std::string_view key, const MessageArguments& arguments = {}) const -> std::string;

  private:
    struct ResolvedMessage {
        const std::string* pattern{};
        std::string locale;
    };
    [[nodiscard]] auto find_template(std::string_view key) const -> ResolvedMessage;

    std::string locale_;
    std::unordered_map<std::string, MessageCatalog> catalogs_;
};

[[nodiscard]] auto english_catalog() -> MessageCatalog;

} // namespace vulpes::core
