#pragma once

#include "vulpes/core/localization.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/ui/document_surface.hpp"

#include <string>
#include <vector>

namespace vulpes::ui {

// A read-only schema inspection surface used by the workspace command palette.
class SchemaDocument final : public DocumentSurface {
  public:
    SchemaDocument(db::TableSchema table, const core::Localizer& messages);

    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult override;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) override;

  private:
    db::TableSchema table_;
    std::string title_;
    std::string footer_;
    std::vector<std::string> lines_;
    std::size_t selected_line_{};
    std::size_t first_visible_line_{};
};

} // namespace vulpes::ui
