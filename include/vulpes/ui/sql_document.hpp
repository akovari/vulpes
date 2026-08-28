#pragma once

#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/ui/document_surface.hpp"
#include "vulpes/ui/grid.hpp"
#include "vulpes/ui/sql_console.hpp"
#include "vulpes/ui/theme.hpp"

#include <optional>

namespace vulpes::ui {

// A workspace-hostable SQL console which reuses the semantic Grid for results.
class SqlDocument final : public DocumentSurface {
  public:
    SqlDocument(db::Database& database, const core::Localizer& messages,
                const Theme& theme = ui::theme(ThemeName::midnight));

    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult override;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) override;

  private:
    void execute();

    db::Database* database_;
    const core::Localizer* messages_;
    const Theme* theme_;
    SqlConsole console_;
    std::optional<GridRows> result_rows_;
    std::optional<Grid> result_grid_;
};

} // namespace vulpes::ui
