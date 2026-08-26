#include "vulpes/core/localization.hpp"

namespace vulpes::core {

Localizer::Localizer(std::string locale) : locale_{std::move(locale)} { add_catalog("en", english_catalog()); }

void Localizer::add_catalog(std::string locale, MessageCatalog catalog) {
    catalogs_.insert_or_assign(std::move(locale), std::move(catalog));
}

auto Localizer::find_template(std::string_view key) const -> const std::string* {
    const auto locate = [&](std::string_view locale) -> const std::string* {
        const auto catalog = catalogs_.find(std::string{locale});
        if (catalog == catalogs_.end()) return nullptr;
        const auto message = catalog->second.find(std::string{key});
        return message == catalog->second.end() ? nullptr : &message->second;
    };
    if (const auto result = locate(locale_)) return result;
    if (const auto separator = locale_.find('-'); separator != std::string::npos) {
        if (const auto result = locate(std::string_view{locale_}.substr(0, separator))) return result;
    }
    return locate("en");
}

auto Localizer::format(std::string_view text, const MessageArguments& arguments) -> std::string {
    std::string result;
    std::size_t position = 0;
    while (position < text.size()) {
        const auto open = text.find('{', position);
        if (open == std::string_view::npos) {
            result.append(text.substr(position));
            break;
        }
        result.append(text.substr(position, open - position));
        const auto close = text.find('}', open + 1);
        if (close == std::string_view::npos) {
            result.append(text.substr(open));
            break;
        }
        const auto key = text.substr(open + 1, close - open - 1);
        if (const auto argument = arguments.find(key); argument != arguments.end()) result += argument->second;
        else result.append(text.substr(open, close - open + 1));
        position = close + 1;
    }
    return result;
}

auto Localizer::translate(std::string_view key, const MessageArguments& arguments) const -> std::string {
    if (const auto text = find_template(key)) return format(*text, arguments);
    return std::string{key};
}

auto english_catalog() -> MessageCatalog {
    return {
        {"application.title", "Vulpes"},
        {"application.unknown_command", "Unknown command. Type help for available commands."},
        {"database.tables", "Tables and views"},
        {"error.unknown_table", "No table or view named '{name}'."},
    };
}

} // namespace vulpes::core

