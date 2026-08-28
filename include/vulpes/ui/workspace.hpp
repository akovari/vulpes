#pragma once

#include "vulpes/core/actions.hpp"
#include "vulpes/core/clipboard.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/confirmation_dialog.hpp"
#include "vulpes/ui/directory_browser.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/text_prompt.hpp"
#include "vulpes/ui/theme.hpp"
#include "vulpes/ui/window_manager.hpp"
#include "vulpes/ui/workspace_text.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vulpes::ui {

enum class WorkspaceResult {
    unchanged,
    redraw,
    open_database,
    open_database_read_only,
    create_database,
    command,
    browse_table,
    run_sql,
    quit,
};

class Workspace {
  public:
    explicit Workspace(WorkspaceText text, const Theme& theme = ui::theme(ThemeName::midnight),
                       core::Clipboard* clipboard = nullptr);

    void set_database(std::string path, std::vector<db::TableSchema> tables, bool read_only = false);
    void set_recent_databases(std::vector<std::string> paths);
    void set_tables(std::vector<db::TableSchema> tables);
    void set_status(std::string status);
    void set_active_document_dirty(bool dirty) noexcept;
    [[nodiscard]] auto requested_path() const -> std::string;
    [[nodiscard]] auto requested_command() const -> std::string;
    [[nodiscard]] auto selected_table() const -> const db::TableSchema*;
    [[nodiscard]] auto active_document() const -> const Document&;
    [[nodiscard]] auto has_document(std::string_view id) const -> bool;
    [[nodiscard]] auto close_active_document() -> bool;
    void open_browse(const db::TableSchema& table);
    void open_schema(const db::TableSchema& table);
    void open_sql_console();
    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> WorkspaceResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    enum class Modal { none, open, open_read_only, create, command };
    enum class Menu { none, file, database, view, window, help };

    void begin_path_prompt(Modal modal);
    void begin_directory_browser();
    void begin_command_prompt();
    void begin_close_confirmation();
    [[nodiscard]] auto menu_item_enabled(Menu menu, std::size_t item) const noexcept -> bool;
    [[nodiscard]] auto activate_menu_item() -> WorkspaceResult;
    WorkspaceText text_;
    std::string database_path_;
    bool database_read_only_{false};
    std::vector<std::string> recent_databases_;
    std::vector<db::TableSchema> tables_;
    std::size_t selected_recent_database_{};
    std::size_t selected_table_{};
    Menu menu_{Menu::none};
    std::size_t menu_selection_{};
    Modal modal_{Modal::none};
    std::optional<TextPrompt> prompt_;
    std::optional<DirectoryBrowser> directory_browser_;
    std::optional<ConfirmationDialog> close_confirmation_;
    std::string submitted_value_;
    const Theme* theme_;
    core::Clipboard* clipboard_;
    WindowManager windows_;
};

} // namespace vulpes::ui
