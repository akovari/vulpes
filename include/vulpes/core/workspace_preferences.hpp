#pragma once

#include "vulpes/core/actions.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace vulpes::core {

// Versioned user-local workspace state. SQLite databases remain self-contained:
// this stores host preferences and never application metadata.
class WorkspacePreferences {
  public:
    static constexpr std::size_t recent_database_limit{10};
    static constexpr std::size_t default_page_size{100};
    static constexpr std::size_t minimum_dataset_page_size{1};
    static constexpr std::size_t maximum_dataset_page_size{1'000};

    [[nodiscard]] static auto load(const std::filesystem::path& path) -> WorkspacePreferences;
    void save(const std::filesystem::path& path) const;
    void add_recent_database(std::filesystem::path path);
    void set_locale(std::string locale);
    void set_theme(std::string theme);
    void set_default_dataset_page_size(std::size_t page_size);
    void set_key_bindings(std::vector<KeyBinding> key_bindings);
    [[nodiscard]] auto recent_databases() const noexcept -> const std::vector<std::filesystem::path>& {
        return recent_databases_;
    }
    [[nodiscard]] auto locale() const noexcept -> const std::string& { return locale_; }
    [[nodiscard]] auto theme() const noexcept -> const std::string& { return theme_; }
    [[nodiscard]] auto default_dataset_page_size() const noexcept -> std::size_t { return default_dataset_page_size_; }
    [[nodiscard]] auto key_bindings() const noexcept -> const std::vector<KeyBinding>& { return key_bindings_; }

  private:
    std::vector<std::filesystem::path> recent_databases_;
    std::string locale_{"en"};
    std::string theme_{"midnight"};
    std::size_t default_dataset_page_size_{default_page_size};
    std::vector<KeyBinding> key_bindings_;
};

// Returns the conventional user-scoped settings path for the current host.
// Callers may offer an explicit path when a portable or test configuration is
// preferred.
[[nodiscard]] auto default_workspace_preferences_path() -> std::filesystem::path;

} // namespace vulpes::core
