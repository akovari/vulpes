#pragma once

#include "vulpes/core/browse_controller.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/ui/confirmation_dialog.hpp"
#include "vulpes/ui/document_surface.hpp"
#include "vulpes/ui/form.hpp"
#include "vulpes/ui/grid.hpp"
#include "vulpes/ui/text_prompt.hpp"
#include "vulpes/ui/theme.hpp"

#include <optional>
#include <string>

namespace vulpes::ui {

// A complete browse surface that can be hosted by the workspace or a future
// frontend. It owns datasets and transient semantic overlays, never a terminal.
class BrowseDocument final : public DocumentSurface {
  public:
    BrowseDocument(db::Database& database, db::TableSchema table, const core::Localizer& messages,
                   const Theme& theme = ui::theme(ThemeName::midnight));

    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult override;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) override;

  private:
    enum class PromptPurpose { none, search, filter };

    void begin_form(FormMode mode);
    void begin_prompt(PromptPurpose purpose);
    void begin_delete_confirmation();
    void apply_prompt();

    const core::Localizer* messages_;
    const Theme* theme_;
    model::Dataset dataset_;
    core::BrowseController controller_;
    Grid grid_;
    std::optional<RecordForm> form_;
    std::optional<TextPrompt> prompt_;
    std::optional<ConfirmationDialog> confirmation_;
    PromptPurpose prompt_purpose_{PromptPurpose::none};
    std::optional<std::pair<std::string, model::SortDirection>> sort_;
};

} // namespace vulpes::ui
