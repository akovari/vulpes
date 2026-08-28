#pragma once

#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vulpes::ui {

enum class DirectoryBrowserResult { unchanged, redraw, selected, cancelled };

// A small filesystem navigator for choosing an existing file. It never creates
// or modifies files; callers still own the meaning and validation of a chosen
// path (for example, opening it as SQLite).
class DirectoryBrowser {
  public:
    DirectoryBrowser(std::filesystem::path initial_directory, std::string title, std::string instructions,
                     std::string parent_label);

    [[nodiscard]] auto selected_path() const -> const std::optional<std::filesystem::path>& { return selected_path_; }
    [[nodiscard]] auto directory() const -> const std::filesystem::path& { return directory_; }
    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> DirectoryBrowserResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    struct Entry {
        std::filesystem::path path;
        std::string label;
        bool directory{false};
        bool parent{false};
    };

    void load(std::filesystem::path directory);
    void move_selection(int direction);
    void select_by_prefix(char32_t character);

    std::filesystem::path directory_;
    std::string title_;
    std::string instructions_;
    std::string parent_label_;
    std::vector<Entry> entries_;
    std::size_t selected_{};
    std::optional<std::filesystem::path> selected_path_;
    std::string error_;
};

} // namespace vulpes::ui
