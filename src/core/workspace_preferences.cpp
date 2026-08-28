#include "vulpes/core/workspace_preferences.hpp"

#include "vulpes/core/error.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <system_error>

namespace vulpes::core {
namespace {

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

    nlohmann::json document;
    try {
        file >> document;
    } catch (const nlohmann::json::exception& error) {
        throw Error{ErrorCategory::metadata, "invalid workspace preferences '" + to_utf8(path) + "': " + error.what()};
    }
    if (!document.is_object() || document.value("version", 0) != 1)
        throw Error{ErrorCategory::metadata, "unsupported workspace preferences '" + to_utf8(path) + "'"};

    WorkspacePreferences preferences;
    if (!document.contains("recent_databases"))
        return preferences;
    const auto& recent = document.at("recent_databases");
    if (!recent.is_array())
        throw Error{ErrorCategory::metadata, "workspace preference 'recent_databases' must be an array"};
    for (auto iterator = recent.rbegin(); iterator != recent.rend(); ++iterator) {
        if (!iterator->is_string())
            throw Error{ErrorCategory::metadata, "workspace preference 'recent_databases' must contain strings"};
        preferences.add_recent_database(from_utf8(iterator->get<std::string>()));
    }
    return preferences;
}

void WorkspacePreferences::save(const std::filesystem::path& path) const {
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error)
            throw_io_error("cannot create workspace preferences directory", parent, error);
    }

    nlohmann::json document{{"version", 1}, {"recent_databases", nlohmann::json::array()}};
    for (const auto& database : recent_databases_)
        document["recent_databases"].push_back(to_utf8(database));

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
