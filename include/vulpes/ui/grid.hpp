#pragma once

#include "vulpes/db/database.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/theme.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vulpes::ui {

// An owning, database-independent tabular input for Grid. It lets the SQL
// console and later reports reuse the same semantic renderer as datasets.
struct GridRows {
    std::vector<db::FieldSchema> fields;
    std::vector<db::Row> rows;

    [[nodiscard]] static auto from_sql_result(db::SqlResult result) -> GridRows;
};

struct GridText {
    std::string empty{"No records"};
    std::string row{"Row"};
    std::string rows{"Rows"};
    std::string column{"Col"};
};

class Grid {
  public:
    Grid(const model::Dataset& dataset, std::string title, std::string footer,
         const Theme& theme = ui::theme(ThemeName::midnight), GridText text = {});
    Grid(const GridRows& rows, std::string title, std::string footer,
         const Theme& theme = ui::theme(ThemeName::midnight), GridText text = {});
    [[nodiscard]] auto move_left() -> bool;
    [[nodiscard]] auto move_right() -> bool;
    [[nodiscard]] auto move_previous_row() -> bool;
    [[nodiscard]] auto move_next_row() -> bool;
    [[nodiscard]] auto resize_selected_column(int delta) -> bool;
    [[nodiscard]] auto selected_column_index() const noexcept -> std::size_t { return selected_column_; }
    [[nodiscard]] auto first_visible_column_index() const noexcept -> std::size_t { return first_visible_column_; }
    [[nodiscard]] auto selected_column_width() const -> std::optional<int>;
    [[nodiscard]] auto selected_field() const -> const db::FieldSchema*;
    void set_focused(bool focused) noexcept { focused_ = focused; }
    void render(terminal::ScreenBuffer& buffer, Rect bounds);

  private:
    const model::Dataset* dataset_;
    const GridRows* rows_{};
    std::string title_;
    std::string footer_;
    const Theme* theme_;
    GridText text_;
    std::size_t selected_column_{};
    std::size_t first_visible_column_{};
    std::optional<std::size_t> selected_result_row_;
    std::size_t first_visible_row_{};
    std::size_t last_dataset_page_offset_{};
    std::unordered_map<std::string, int> column_width_overrides_;
    std::unordered_map<std::string, int> rendered_column_widths_;
    bool focused_{true};
};

} // namespace vulpes::ui
