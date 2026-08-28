#include "vulpes/core/error.hpp"
#include "vulpes/core/workspace_preferences.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory()
        : path_{std::filesystem::temp_directory_path() /
                ("vulpes-preferences-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))} {
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("workspace preferences persist a bounded, deduplicated recent database list", "[core][preferences]") {
    TemporaryDirectory temporary;
    const auto settings_path = temporary.path() / "nested" / "settings.json";
    vulpes::core::WorkspacePreferences preferences;

    preferences.add_recent_database("first.db");
    preferences.add_recent_database("second.db");
    preferences.add_recent_database("first.db");
    preferences.save(settings_path);

    const auto loaded = vulpes::core::WorkspacePreferences::load(settings_path);
    REQUIRE(loaded.recent_databases().size() == 2);
    CHECK(loaded.recent_databases().at(0) == std::filesystem::path{"first.db"});
    CHECK(loaded.recent_databases().at(1) == std::filesystem::path{"second.db"});
}

TEST_CASE("workspace preferences reject malformed documents", "[core][preferences]") {
    TemporaryDirectory temporary;
    const auto settings_path = temporary.path() / "settings.json";
    std::filesystem::create_directories(temporary.path());
    std::ofstream file{settings_path};
    file << "{ invalid";
    file.close();

    CHECK_THROWS_AS(vulpes::core::WorkspacePreferences::load(settings_path), vulpes::Error);
}

TEST_CASE("workspace preferences preserve Unicode recent database paths", "[core][preferences]") {
    TemporaryDirectory temporary;
    const auto settings_path = temporary.path() / "settings.json";
    const std::filesystem::path database_path{u8"žluťoučký.db"};
    vulpes::core::WorkspacePreferences preferences;

    preferences.add_recent_database(database_path);
    preferences.save(settings_path);

    const auto loaded = vulpes::core::WorkspacePreferences::load(settings_path);
    REQUIRE(loaded.recent_databases().size() == 1);
    CHECK(loaded.recent_databases().front() == database_path);
}
