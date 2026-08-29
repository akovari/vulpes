#pragma once

#include "vulpes/appmeta/metadata.hpp"
#include "vulpes/core/formatting.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/theme.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vulpes::ui {

// An owning, database-independent tabular input for Grid. It lets the SQL
// console and later reports reuse the same semantic renderer as datasets.
struct GridRows {
    std::vector<db::FieldSchema> fields;
    std::vector<db::Row> rows;
    bool truncated{false};

    [[nodiscard]] static auto from_sql_result(db::SqlResult result) -> GridRows;
};

struct GridText {
    std::string empty{"No records"};
    std::string row{"Row"};
    std::string rows{"Rows"};
    std::string column{"Col"};
};

// A calculation is a presentation projection over an already-owned row. It
// cannot become a writable Dataset field or issue a database operation.
struct GridCalculatedColumn {
    std::string id;
    std::string label;
    std::function<db::Value(const db::Row&)> value;
};

struct GridAggregation {
    model::AggregateDefinition definition;
    std::string label;
};

// Grid options are semantic layout/data projections, never terminal
// coordinates. They can be shared with a later GUI or web Grid renderer.
struct GridOptions {
    std::size_t frozen_columns{};
    std::vector<GridCalculatedColumn> calculated_columns;
    std::vector<GridAggregation> aggregations;
};

class Grid {
  public:
    Grid(const model::Dataset& dataset, std::string title, std::string footer,
         const Theme& theme = ui::theme(ThemeName::midnight), GridText text = {},
         std::optional<core::LocaleFormatter> formatter = std::nullopt,
         std::optional<appmeta::TableMetadata> metadata = std::nullopt, GridOptions options = {});
    Grid(const GridRows& rows, std::string title, std::string footer,
         const Theme& theme = ui::theme(ThemeName::midnight), GridText text = {},
         std::optional<core::LocaleFormatter> formatter = std::nullopt,
         std::optional<appmeta::TableMetadata> metadata = std::nullopt, GridOptions options = {});
    Grid(GridRows& rows, std::string title, std::string footer, const Theme& theme = ui::theme(ThemeName::midnight),
         GridText text = {}, std::optional<core::LocaleFormatter> formatter = std::nullopt,
         std::optional<appmeta::TableMetadata> metadata = std::nullopt, GridOptions options = {});
    [[nodiscard]] auto move_left() -> bool;
    [[nodiscard]] auto move_right() -> bool;
    [[nodiscard]] auto move_previous_row() -> bool;
    [[nodiscard]] auto move_next_row() -> bool;
    [[nodiscard]] auto resize_selected_column(int delta) -> bool;
    [[nodiscard]] auto sort_selected() -> bool;
    [[nodiscard]] auto selected_column_index() const noexcept -> std::size_t { return selected_column_; }
    [[nodiscard]] auto first_visible_column_index() const noexcept -> std::size_t { return first_visible_column_; }
    [[nodiscard]] auto selected_column_width() const -> std::optional<int>;
    [[nodiscard]] auto selected_field() const -> const db::FieldSchema*;
    [[nodiscard]] auto selected_column_is_read_only() const -> bool;
    [[nodiscard]] auto aggregation_results() const -> const std::vector<model::AggregateResult>&;
    void set_options(GridOptions options);
    void set_focused(bool focused) noexcept { focused_ = focused; }
    void render(terminal::ScreenBuffer& buffer, Rect bounds);

  private:
    struct DisplayColumn;

    [[nodiscard]] auto display_columns() const -> std::vector<DisplayColumn>;
    [[nodiscard]] auto aggregation_definitions() const -> std::vector<model::AggregateDefinition>;
    [[nodiscard]] auto calculate_row_aggregates() const -> std::vector<model::AggregateResult>;
    void validate_options() const;

    const model::Dataset* dataset_{};
    const GridRows* rows_{};
    GridRows* mutable_rows_{};
    std::string title_;
    std::string footer_;
    const Theme* theme_;
    std::optional<core::LocaleFormatter> formatter_;
    std::optional<appmeta::TableMetadata> metadata_;
    GridOptions options_;
    GridText text_;
    std::size_t selected_column_{};
    std::size_t first_visible_column_{};
    std::optional<std::size_t> selected_result_row_;
    std::optional<std::pair<std::string, bool>> result_sort_;
    std::size_t first_visible_row_{};
    std::size_t last_dataset_page_offset_{};
    std::unordered_map<std::string, int> column_width_overrides_;
    std::unordered_map<std::string, int> rendered_column_widths_;
    mutable std::vector<model::AggregateResult> aggregate_results_;
    mutable std::size_t aggregate_dataset_revision_{static_cast<std::size_t>(-1)};
    bool focused_{true};
};

} // namespace vulpes::ui
