#pragma once

#include "vulpes/appmeta/metadata.hpp"
#include "vulpes/core/clipboard.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/line_editor.hpp"
#include "vulpes/ui/theme.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace vulpes::ui {

enum class RelationshipLookupResult { unchanged, redraw, selected, cancelled, drill_down };

class RelationshipLookup {
  public:
    RelationshipLookup(model::Dataset& dataset, std::string field, model::LookupQuery query, bool allow_drill_down,
                       std::string title, std::string search_label, std::string instructions,
                       const Theme& theme = ui::theme(ThemeName::midnight), core::Clipboard* clipboard = nullptr);

    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> RelationshipLookupResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

    [[nodiscard]] auto field() const noexcept -> std::string_view { return field_; }
    [[nodiscard]] auto options() const noexcept -> const std::vector<model::LookupOption>& { return options_; }
    [[nodiscard]] auto selected_index() const noexcept -> std::size_t { return selected_; }
    [[nodiscard]] auto selected_option() const noexcept -> const model::LookupOption*;

  private:
    void refresh();
    void move_selection(int direction);

    model::Dataset* dataset_;
    std::string field_;
    model::LookupQuery query_;
    bool allow_drill_down_;
    std::string title_;
    std::string search_label_;
    std::string instructions_;
    LineEditor search_;
    std::vector<model::LookupOption> options_;
    std::size_t selected_{};
    std::string error_;
    const Theme* theme_;
    core::Clipboard* clipboard_;
};

enum class RelatedRecordResult { unchanged, redraw, cancelled };

class RelatedRecordView {
  public:
    RelatedRecordView(model::RelatedRecord record, std::string title, std::string instructions,
                      const Theme& theme = ui::theme(ThemeName::midnight),
                      const appmeta::TableMetadata* metadata = nullptr);

    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> RelatedRecordResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

    [[nodiscard]] auto field_count() const noexcept -> std::size_t { return fields_.size(); }

  private:
    std::string title_;
    std::string instructions_;
    std::vector<std::pair<std::string, std::string>> fields_;
    std::size_t selected_{};
    const Theme* theme_;
};

} // namespace vulpes::ui
