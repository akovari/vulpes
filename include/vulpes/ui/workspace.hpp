#pragma once

#include "vulpes/core/actions.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/text_prompt.hpp"
#include "vulpes/ui/window_manager.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vulpes::ui {

enum class WorkspaceResult { unchanged, redraw, open_database, create_database, browse_table, run_sql, quit };

class Workspace {
  public:
    Workspace(std::string title, std::string open_label, std::string create_label, std::string path_instructions);

    void set_database(std::string path, std::vector<db::TableSchema> tables);
    void set_status(std::string status);
    [[nodiscard]] auto requested_path() const -> std::string;
    [[nodiscard]] auto selected_table() const -> const db::TableSchema*;
    [[nodiscard]] auto active_document() const -> const Document&;
    [[nodiscard]] auto has_document(std::string_view id) const -> bool;
    [[nodiscard]] auto close_active_document() -> bool;
    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> WorkspaceResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    enum class Modal { none, open, create };
    enum class Menu { none, file, database, view, window, help };

    void begin_path_prompt(Modal modal);
    [[nodiscard]] auto activate_menu_item() -> WorkspaceResult;
    std::string title_;
    std::string open_label_;
    std::string create_label_;
    std::string path_instructions_;
    std::string database_path_;
    std::string status_;
    std::vector<db::TableSchema> tables_;
    std::size_t selected_table_{};
    Menu menu_{Menu::none};
    std::size_t menu_selection_{};
    Modal modal_{Modal::none};
    std::optional<TextPrompt> prompt_;
    WindowManager windows_;
};

} // namespace vulpes::ui
