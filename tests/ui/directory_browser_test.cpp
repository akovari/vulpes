#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/directory_browser.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory()
        : path_{std::filesystem::temp_directory_path() /
                ("vulpes-directory-browser-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))} {
        std::filesystem::create_directories(path_ / "child");
        std::ofstream{path_ / "ledger.db"} << "SQLite format 3";
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

TEST_CASE("directory browser navigates directories and selects existing files", "[ui][directory]") {
    TemporaryDirectory temporary;
    vulpes::ui::DirectoryBrowser browser{temporary.path(), "Open file", "Enter Open  Esc Cancel", "[..]"};

    CHECK(browser.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}) ==
          vulpes::ui::DirectoryBrowserResult::redraw);
    CHECK(browser.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::DirectoryBrowserResult::redraw);
    CHECK(browser.directory().filename() == "child");
    CHECK(browser.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::backspace}) ==
          vulpes::ui::DirectoryBrowserResult::redraw);
    CHECK(browser.directory() == std::filesystem::absolute(temporary.path()));

    static_cast<void>(browser.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::end}));
    CHECK(browser.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::DirectoryBrowserResult::selected);
    REQUIRE(browser.selected_path());
    CHECK(browser.selected_path()->filename() == "ledger.db");

    vulpes::terminal::ScreenBuffer buffer{60, 15};
    browser.render(buffer, {0, 0, 60, 15});
    CHECK(buffer.cell(3, 0).glyph == U'O');
    CHECK(buffer.cell(60 - 1, 14).glyph == U'┘');
}
