#include "vulpes/core/workspace_preferences.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace vulpes::core {
namespace {

constexpr int workspace_preferences_version{2};

auto to_utf8(const std::filesystem::path& path) -> std::string {
    const auto encoded = path.generic_u8string();
    return {encoded.begin(), encoded.end()};
}

auto from_utf8(const std::string& value) -> std::filesystem::path {
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const auto character : value)
        encoded.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    return {encoded};
}

void throw_io_error(std::string operation, const std::filesystem::path& path, const std::error_code& error) {
    throw Error{ErrorCategory::io,
                std::move(operation) + " '" + to_utf8(path) + "': " + (error ? error.message() : "I/O error")};
}

auto environment_path(const char* name) -> std::filesystem::path {
#ifdef _WIN32
    std::wstring variable;
    variable.reserve(std::strlen(name));
    for (const auto* character = name; *character != '\0'; ++character)
        variable.push_back(static_cast<wchar_t>(*character));

    wchar_t* value{};
    std::size_t length{};
    if (_wdupenv_s(&value, &length, variable.c_str()) != 0 || value == nullptr)
        return {};
    const std::filesystem::path path{value};
    std::free(value);
    return path;
#else
    const auto* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? std::filesystem::path{} : from_utf8(value);
#endif
}

auto key_name(terminal::Key key) -> std::string_view {
    using terminal::Key;
    switch (key) {
    case Key::character:
        return "character";
    case Key::enter:
        return "enter";
    case Key::escape:
        return "escape";
    case Key::tab:
        return "tab";
    case Key::backspace:
        return "backspace";
    case Key::up:
        return "up";
    case Key::down:
        return "down";
    case Key::left:
        return "left";
    case Key::right:
        return "right";
    case Key::home:
        return "home";
    case Key::end:
        return "end";
    case Key::page_up:
        return "page_up";
    case Key::page_down:
        return "page_down";
    case Key::insert_key:
        return "insert";
    case Key::delete_key:
        return "delete";
    case Key::f1:
        return "f1";
    case Key::f2:
        return "f2";
    case Key::f3:
        return "f3";
    case Key::f4:
        return "f4";
    case Key::f5:
        return "f5";
    case Key::f6:
        return "f6";
    case Key::f7:
        return "f7";
    case Key::f8:
        return "f8";
    case Key::f9:
        return "f9";
    case Key::f10:
        return "f10";
    case Key::f11:
        return "f11";
    case Key::f12:
        return "f12";
    case Key::unknown:
        return {};
    }
    return {};
}

auto key_from_name(std::string_view name) -> std::optional<terminal::Key> {
    using terminal::Key;
    constexpr std::array keys{
        std::pair{"character", Key::character},
        std::pair{"enter", Key::enter},
        std::pair{"escape", Key::escape},
        std::pair{"tab", Key::tab},
        std::pair{"backspace", Key::backspace},
        std::pair{"up", Key::up},
        std::pair{"down", Key::down},
        std::pair{"left", Key::left},
        std::pair{"right", Key::right},
        std::pair{"home", Key::home},
        std::pair{"end", Key::end},
        std::pair{"page_up", Key::page_up},
        std::pair{"page_down", Key::page_down},
        std::pair{"insert", Key::insert_key},
        std::pair{"delete", Key::delete_key},
        std::pair{"f1", Key::f1},
        std::pair{"f2", Key::f2},
        std::pair{"f3", Key::f3},
        std::pair{"f4", Key::f4},
        std::pair{"f5", Key::f5},
        std::pair{"f6", Key::f6},
        std::pair{"f7", Key::f7},
        std::pair{"f8", Key::f8},
        std::pair{"f9", Key::f9},
        std::pair{"f10", Key::f10},
        std::pair{"f11", Key::f11},
        std::pair{"f12", Key::f12},
    };
    const auto key = std::ranges::find_if(keys, [&](const auto& item) { return item.first == name; });
    return key == keys.end() ? std::nullopt : std::optional{key->second};
}

auto same_key(const terminal::KeyEvent& left, const terminal::KeyEvent& right) -> bool {
    return left.key == right.key && left.character == right.character && left.ctrl == right.ctrl &&
           left.alt == right.alt && left.shift == right.shift;
}

void validate_text_preference(std::string_view value, std::string_view name) {
    if (value.empty())
        throw Error{ErrorCategory::validation, "workspace preference '" + std::string{name} + "' cannot be empty"};
}

void validate_page_size(std::size_t page_size) {
    if (page_size < WorkspacePreferences::minimum_dataset_page_size ||
        page_size > WorkspacePreferences::maximum_dataset_page_size) {
        throw Error{ErrorCategory::validation, "workspace preference 'default_dataset_page_size' must be between " +
                                                   std::to_string(WorkspacePreferences::minimum_dataset_page_size) +
                                                   " and " +
                                                   std::to_string(WorkspacePreferences::maximum_dataset_page_size)};
    }
}

void validate_key_binding(const KeyBinding& binding) {
    if (binding.action == ActionId::none)
        throw Error{ErrorCategory::validation, "workspace key bindings cannot target an empty action"};
    if (key_name(binding.key.key).empty())
        throw Error{ErrorCategory::validation, "workspace key bindings cannot use an unknown key"};
    if (binding.key.key == terminal::Key::character && binding.key.character == U'\0')
        throw Error{ErrorCategory::validation, "character key bindings require one character"};
}

auto require_string(const nlohmann::json& object, std::string_view name) -> std::string {
    const auto field = object.find(std::string{name});
    if (field == object.end() || !field->is_string())
        throw Error{ErrorCategory::metadata, "workspace preference '" + std::string{name} + "' must be a string"};
    return field->get<std::string>();
}

auto optional_boolean(const nlohmann::json& object, std::string_view name) -> bool {
    const auto field = object.find(std::string{name});
    if (field == object.end())
        return false;
    if (!field->is_boolean())
        throw Error{ErrorCategory::metadata, "workspace key binding '" + std::string{name} + "' must be a boolean"};
    return field->get<bool>();
}

auto parse_key_binding(const nlohmann::json& document) -> KeyBinding {
    if (!document.is_object())
        throw Error{ErrorCategory::metadata, "workspace preference 'key_bindings' must contain objects"};
    const auto action = action_from_id(require_string(document, "action"));
    if (!action)
        throw Error{ErrorCategory::metadata, "workspace key binding has an unknown action"};
    const auto key = key_from_name(require_string(document, "key"));
    if (!key)
        throw Error{ErrorCategory::metadata, "workspace key binding has an unknown key"};

    terminal::KeyEvent event{.key = *key,
                             .ctrl = optional_boolean(document, "ctrl"),
                             .alt = optional_boolean(document, "alt"),
                             .shift = optional_boolean(document, "shift")};
    if (*key == terminal::Key::character) {
        const auto character = require_string(document, "character");
        const auto code_point = terminal::first_code_point(character);
        if (code_point == U'\0' || terminal::encode_utf8(code_point) != character)
            throw Error{ErrorCategory::metadata,
                        "workspace character key bindings require exactly one UTF-8 character"};
        event.character = code_point;
    } else if (document.contains("character")) {
        throw Error{ErrorCategory::metadata, "only character key bindings may define 'character'"};
    }
    return {.key = event, .action = *action};
}

auto serialize_key_binding(const KeyBinding& binding) -> nlohmann::json {
    validate_key_binding(binding);
    nlohmann::json document{{"action", action_id(binding.action)},
                            {"key", key_name(binding.key.key)},
                            {"ctrl", binding.key.ctrl},
                            {"alt", binding.key.alt},
                            {"shift", binding.key.shift}};
    if (binding.key.key == terminal::Key::character)
        document["character"] = terminal::encode_utf8(binding.key.character);
    return document;
}

void load_recent_databases(WorkspacePreferences& preferences, const nlohmann::json& document) {
    if (!document.contains("recent_databases"))
        return;
    const auto& recent = document.at("recent_databases");
    if (!recent.is_array())
        throw Error{ErrorCategory::metadata, "workspace preference 'recent_databases' must be an array"};
    for (auto iterator = recent.rbegin(); iterator != recent.rend(); ++iterator) {
        if (!iterator->is_string())
            throw Error{ErrorCategory::metadata, "workspace preference 'recent_databases' must contain strings"};
        preferences.add_recent_database(from_utf8(iterator->get<std::string>()));
    }
}

} // namespace

auto WorkspacePreferences::load(const std::filesystem::path& path) -> WorkspacePreferences {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error)
        throw_io_error("cannot inspect workspace preferences", path, error);
    if (!exists)
        return {};

    std::ifstream file{path, std::ios::binary};
    if (!file)
        throw Error{ErrorCategory::io, "cannot read workspace preferences '" + to_utf8(path) + "'"};

    try {
        nlohmann::json document;
        file >> document;
        if (!document.is_object() || !document.contains("version") || !document.at("version").is_number_integer())
            throw Error{ErrorCategory::metadata, "unsupported workspace preferences '" + to_utf8(path) + "'"};
        const auto version = document.at("version").get<int>();
        if (version != 1 && version != workspace_preferences_version)
            throw Error{ErrorCategory::metadata, "unsupported workspace preferences '" + to_utf8(path) + "'"};

        WorkspacePreferences preferences;
        load_recent_databases(preferences, document);
        if (version == 1)
            return preferences;

        if (document.contains("locale"))
            preferences.set_locale(require_string(document, "locale"));
        if (document.contains("theme"))
            preferences.set_theme(require_string(document, "theme"));
        if (document.contains("default_dataset_page_size")) {
            const auto& page_size = document.at("default_dataset_page_size");
            if (!page_size.is_number_unsigned()) {
                throw Error{ErrorCategory::metadata,
                            "workspace preference 'default_dataset_page_size' must be an unsigned integer"};
            }
            preferences.set_default_dataset_page_size(page_size.get<std::size_t>());
        }
        if (document.contains("key_bindings")) {
            const auto& bindings = document.at("key_bindings");
            if (!bindings.is_array())
                throw Error{ErrorCategory::metadata, "workspace preference 'key_bindings' must be an array"};
            std::vector<KeyBinding> parsed;
            parsed.reserve(bindings.size());
            for (const auto& binding : bindings)
                parsed.push_back(parse_key_binding(binding));
            preferences.set_key_bindings(std::move(parsed));
        }
        return preferences;
    } catch (const nlohmann::json::exception& error) {
        throw Error{ErrorCategory::metadata, "invalid workspace preferences '" + to_utf8(path) + "': " + error.what()};
    }
}

void WorkspacePreferences::save(const std::filesystem::path& path) const {
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error)
            throw_io_error("cannot create workspace preferences directory", parent, error);
    }

    nlohmann::json document{{"version", workspace_preferences_version},
                            {"recent_databases", nlohmann::json::array()},
                            {"locale", locale_},
                            {"theme", theme_},
                            {"default_dataset_page_size", default_dataset_page_size_},
                            {"key_bindings", nlohmann::json::array()}};
    for (const auto& database : recent_databases_)
        document["recent_databases"].push_back(to_utf8(database));
    for (const auto& binding : key_bindings_)
        document["key_bindings"].push_back(serialize_key_binding(binding));

    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file)
        throw Error{ErrorCategory::io, "cannot write workspace preferences '" + to_utf8(path) + "'"};
    file << document.dump(2) << '\n';
    file.flush();
    if (!file)
        throw Error{ErrorCategory::io, "cannot finish writing workspace preferences '" + to_utf8(path) + "'"};
}

void WorkspacePreferences::add_recent_database(std::filesystem::path path) {
    if (path.empty())
        return;
    path = path.lexically_normal();
    std::erase(recent_databases_, path);
    recent_databases_.insert(recent_databases_.begin(), std::move(path));
    if (recent_databases_.size() > recent_database_limit)
        recent_databases_.resize(recent_database_limit);
}

void WorkspacePreferences::set_locale(std::string locale) {
    validate_text_preference(locale, "locale");
    locale_ = std::move(locale);
}

void WorkspacePreferences::set_theme(std::string theme) {
    validate_text_preference(theme, "theme");
    theme_ = std::move(theme);
}

void WorkspacePreferences::set_default_dataset_page_size(std::size_t page_size) {
    validate_page_size(page_size);
    default_dataset_page_size_ = page_size;
}

void WorkspacePreferences::set_key_bindings(std::vector<KeyBinding> key_bindings) {
    for (auto first = key_bindings.begin(); first != key_bindings.end(); ++first) {
        validate_key_binding(*first);
        if (std::ranges::find_if(std::next(first), key_bindings.end(), [&](const auto& second) {
                return same_key(first->key, second.key);
            }) != key_bindings.end()) {
            throw Error{ErrorCategory::validation, "workspace key bindings must not define the same key twice"};
        }
    }
    key_bindings_ = std::move(key_bindings);
}

auto default_workspace_preferences_path() -> std::filesystem::path {
#ifdef _WIN32
    auto base = environment_path("APPDATA");
    if (base.empty())
        base = environment_path("LOCALAPPDATA");
    if (base.empty())
        throw Error{ErrorCategory::io, "cannot determine the Windows application-data directory"};
    return base / "Vulpes" / "settings.json";
#elif defined(__APPLE__)
    const auto home = environment_path("HOME");
    if (home.empty())
        throw Error{ErrorCategory::io, "cannot determine the macOS home directory"};
    return home / "Library" / "Application Support" / "Vulpes" / "settings.json";
#else
    auto base = environment_path("XDG_CONFIG_HOME");
    if (base.empty()) {
        const auto home = environment_path("HOME");
        if (home.empty())
            throw Error{ErrorCategory::io, "cannot determine the XDG configuration directory"};
        base = home / ".config";
    }
    return base / "vulpes" / "settings.json";
#endif
}

} // namespace vulpes::core
