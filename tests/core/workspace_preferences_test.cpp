#include "vulpes/core/error.hpp"
#include "vulpes/core/workspace_preferences.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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

TEST_CASE("workspace preferences migrate v1 recent databases to versioned host settings", "[core][preferences]") {
    TemporaryDirectory temporary;
    const auto settings_path = temporary.path() / "settings.json";
    std::filesystem::create_directories(temporary.path());
    {
        std::ofstream file{settings_path};
        file << R"({"version":1,"recent_databases":["older.db","newer.db"]})";
    }

    auto preferences = vulpes::core::WorkspacePreferences::load(settings_path);
    CHECK(preferences.locale() == "en");
    CHECK(preferences.theme() == "midnight");
    CHECK(preferences.default_dataset_page_size() == vulpes::core::WorkspacePreferences::default_page_size);
    CHECK(preferences.key_bindings().empty());
    REQUIRE(preferences.recent_databases().size() == 2);
    CHECK(preferences.recent_databases().front() == std::filesystem::path{"older.db"});

    preferences.save(settings_path);
    std::ifstream saved{settings_path};
    const std::string document{std::istreambuf_iterator<char>{saved}, std::istreambuf_iterator<char>{}};
    CHECK(document.find("\"version\": 2") != std::string::npos);
}

TEST_CASE("workspace preferences persist presentation defaults and semantic key binding overrides",
          "[core][preferences]") {
    TemporaryDirectory temporary;
    const auto settings_path = temporary.path() / "settings.json";
    vulpes::core::WorkspacePreferences preferences;
    preferences.set_locale("cs-CZ");
    preferences.set_theme("high-contrast");
    preferences.set_default_dataset_page_size(48);
    preferences.set_key_bindings({
        {.key = {.key = vulpes::terminal::Key::f9}, .action = vulpes::core::ActionId::record_edit},
        {.key = {.key = vulpes::terminal::Key::character, .character = U'\u00E9', .alt = true},
         .action = vulpes::core::ActionId::application_menu},
    });
    preferences.save(settings_path);

    const auto loaded = vulpes::core::WorkspacePreferences::load(settings_path);
    CHECK(loaded.locale() == "cs-CZ");
    CHECK(loaded.theme() == "high-contrast");
    CHECK(loaded.default_dataset_page_size() == 48);
    REQUIRE(loaded.key_bindings().size() == 2);
    CHECK(loaded.key_bindings().at(1).key.character == U'\u00E9');
    CHECK(loaded.key_bindings().at(1).key.alt);

    vulpes::core::ActionMap actions;
    for (const auto& binding : loaded.key_bindings())
        actions.bind(binding);
    CHECK(actions.action_for(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f9}) ==
          vulpes::core::ActionId::record_edit);
    CHECK(actions.action_for(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character,
                                                        .character = U'\u00E9',
                                                        .alt = true}) == vulpes::core::ActionId::application_menu);
}

TEST_CASE("workspace preferences reject unsafe page sizes and duplicate key bindings", "[core][preferences]") {
    vulpes::core::WorkspacePreferences preferences;
    CHECK_THROWS_AS(preferences.set_default_dataset_page_size(0), vulpes::Error);
    CHECK_THROWS_AS(preferences.set_default_dataset_page_size(1'001), vulpes::Error);
    CHECK_THROWS_AS(preferences.set_key_bindings({
                        {.key = {.key = vulpes::terminal::Key::f9}, .action = vulpes::core::ActionId::record_edit},
                        {.key = {.key = vulpes::terminal::Key::f9}, .action = vulpes::core::ActionId::dataset_refresh},
                    }),
                    vulpes::Error);
}
