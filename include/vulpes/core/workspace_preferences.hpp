#pragma once

#include <filesystem>
#include <vector>

namespace vulpes::core {

// User-local workspace state. SQLite databases remain self-contained; this
// stores only host preferences such as the bounded recent-file history.
class WorkspacePreferences {
  public:
    static constexpr std::size_t recent_database_limit{10};

    [[nodiscard]] static auto load(const std::filesystem::path& path) -> WorkspacePreferences;
    void save(const std::filesystem::path& path) const;
    void add_recent_database(std::filesystem::path path);
    [[nodiscard]] auto recent_databases() const noexcept -> const std::vector<std::filesystem::path>& {
        return recent_databases_;
    }

  private:
    std::vector<std::filesystem::path> recent_databases_;
};

// Returns the conventional user-scoped settings path for the current host.
// Callers may offer an explicit path when a portable or test configuration is
// preferred.
[[nodiscard]] auto default_workspace_preferences_path() -> std::filesystem::path;

} // namespace vulpes::core
