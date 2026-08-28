#pragma once

#include "vulpes/appmeta/definition.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/ui/document_surface.hpp"
#include "vulpes/ui/grid.hpp"
#include "vulpes/ui/theme.hpp"

namespace vulpes::ui {

// A named report is one validated, read-only SQL query presented through Grid.
// The document owns all result rows and has no editing or SQLite mutation path.
class ReportDocument final : public DocumentSurface {
  public:
    ReportDocument(db::Database& database, appmeta::ReportDefinition report, const core::Localizer& messages,
                   const Theme& theme = ui::theme(ThemeName::midnight));

    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult override;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) override;

  private:
    GridRows rows_;
    Grid grid_;
};

} // namespace vulpes::ui
