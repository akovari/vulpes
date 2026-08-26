#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace vulpes::core {

using MessageArguments = std::map<std::string, std::string, std::less<>>;
using MessageCatalog = std::unordered_map<std::string, std::string>;

class Localizer {
  public:
    explicit Localizer(std::string locale = "en");
    void add_catalog(std::string locale, MessageCatalog catalog);
    void load_catalog_file(const std::filesystem::path& path);
    [[nodiscard]] auto locale() const noexcept -> std::string_view { return locale_; }
    [[nodiscard]] auto translate(std::string_view key, const MessageArguments& arguments = {}) const -> std::string;

  private:
    [[nodiscard]] auto find_template(std::string_view key) const -> const std::string*;
    [[nodiscard]] static auto format(std::string_view text, const MessageArguments& arguments) -> std::string;

    std::string locale_;
    std::unordered_map<std::string, MessageCatalog> catalogs_;
};

[[nodiscard]] auto english_catalog() -> MessageCatalog;

} // namespace vulpes::core
