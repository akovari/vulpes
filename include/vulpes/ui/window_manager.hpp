#pragma once

#include "vulpes/core/actions.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/theme.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace vulpes::ui {

enum class DocumentKind { workspace, browse, schema, sql_console };

struct Document {
    std::string id;
    std::string title;
    DocumentKind kind{DocumentKind::workspace};
    bool closable{true};
};

// Owns only terminal workspace chrome and document/modal state. Document
// content remains semantic UI supplied by the application shell.
class WindowManager {
  public:
    WindowManager(const Theme& theme, std::string workspace_title);

    void open(Document document);
    [[nodiscard]] auto close_active() -> bool;
    [[nodiscard]] auto active() const -> const Document&;
    [[nodiscard]] auto documents() const noexcept -> const std::vector<Document>& { return documents_; }
    [[nodiscard]] auto active_index() const noexcept -> std::size_t { return active_index_; }
    void show_modal(std::string title);
    void dismiss_modal() noexcept;
    [[nodiscard]] auto modal_title() const noexcept -> const std::optional<std::string>& { return modal_title_; }
    [[nodiscard]] auto handle(core::ActionId action) -> bool;
    void render_tabs(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    const Theme* theme_;
    std::vector<Document> documents_;
    std::size_t active_index_{};
    std::optional<std::string> modal_title_;
};

} // namespace vulpes::ui
