#include "vulpes/ui/grid.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <charconv>
#include <variant>

namespace vulpes::ui {
namespace {

auto display_value(const db::Value& value) -> std::string {
    return std::visit(
        [](const auto& item) -> std::string {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return {};
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return std::to_string(item);
            else if constexpr (std::is_same_v<T, double>) {
                char output[64]{};
                const auto [end, error] = std::to_chars(std::begin(output), std::end(output), item);
                return error == std::errc{} ? std::string{output, end} : "?";
            } else if constexpr (std::is_same_v<T, std::string>)
                return item;
            else
                return "<blob " + std::to_string(item.size()) + " bytes>";
        },
        value.storage());
}

void write_padded(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text,
                  terminal::Style style = {}) {
    const auto clipped = terminal::truncate_utf8(text, width);
    const auto end = buffer.write_utf8(x, y, clipped, style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

auto GridRows::from_sql_result(db::SqlResult result) -> GridRows {
    GridRows grid_rows;
    grid_rows.fields.reserve(result.columns.size());
    for (auto& column : result.columns)
        grid_rows.fields.push_back({.name = std::move(column)});
    grid_rows.rows = std::move(result.rows);
    return grid_rows;
}

Grid::Grid(const model::Dataset& dataset, std::string title, std::string footer)
    : dataset_{&dataset}, title_{std::move(title)}, footer_{std::move(footer)} {
}

Grid::Grid(const GridRows& rows, std::string title, std::string footer)
    : dataset_{}, rows_{&rows}, title_{std::move(title)}, footer_{std::move(footer)} {
    if (!rows.rows.empty())
        selected_result_row_ = 0;
}

auto Grid::selected_field() const -> const db::FieldSchema* {
    std::size_t visible_index{};
    const auto& fields = dataset_ != nullptr ? dataset_->schema().fields : rows_->fields;
    for (const auto& field : fields) {
        if (field.hidden)
            continue;
        if (visible_index == selected_column_)
            return &field;
        ++visible_index;
    }
    return nullptr;
}

auto Grid::move_left() -> bool {
    if (selected_column_ == 0)
        return false;
    --selected_column_;
    first_visible_column_ = std::min(first_visible_column_, selected_column_);
    return true;
}

auto Grid::move_right() -> bool {
    std::size_t field_count{};
    const auto& fields = dataset_ != nullptr ? dataset_->schema().fields : rows_->fields;
    for (const auto& field : fields) {
        if (!field.hidden)
            ++field_count;
    }
    if (selected_column_ + 1 >= field_count)
        return false;
    ++selected_column_;
    return true;
}

auto Grid::move_previous_row() -> bool {
    if (rows_ == nullptr || !selected_result_row_ || *selected_result_row_ == 0)
        return false;
    --*selected_result_row_;
    return true;
}

auto Grid::move_next_row() -> bool {
    if (rows_ == nullptr || !selected_result_row_ || *selected_result_row_ + 1 >= rows_->rows.size())
        return false;
    ++*selected_result_row_;
    return true;
}

void Grid::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    if (bounds.width < 4 || bounds.height < 6 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height())
        return;

    std::vector<const db::FieldSchema*> all_fields;
    const auto& schema_fields = dataset_ != nullptr ? dataset_->schema().fields : rows_->fields;
    for (const auto& field : schema_fields)
        if (!field.hidden)
            all_fields.push_back(&field);
    if (all_fields.empty())
        return;

    const int interior_width = bounds.width - 2;
    auto visible_fields = [&](std::size_t first_visible) {
        std::vector<const db::FieldSchema*> fields;
        constexpr int minimum_column_width = 4;
        for (std::size_t index = first_visible; index < all_fields.size(); ++index) {
            const auto candidate_count = static_cast<int>(fields.size()) + 1;
            const int candidate_content_width = interior_width - candidate_count + 1;
            if (candidate_content_width / candidate_count < minimum_column_width)
                break;
            fields.push_back(all_fields[index]);
        }
        return fields;
    };

    auto first_visible = std::min(first_visible_column_, all_fields.size() - 1);
    auto fields = visible_fields(first_visible);
    while (selected_column_ >= first_visible + fields.size() && first_visible < selected_column_) {
        ++first_visible;
        fields = visible_fields(first_visible);
    }
    first_visible_column_ = first_visible;
    if (fields.empty())
        return;

    const int content_width = interior_width - static_cast<int>(fields.size()) + 1;
    if (content_width < static_cast<int>(fields.size()))
        return;
    const int base_width = content_width / static_cast<int>(fields.size());
    const int remainder = content_width % static_cast<int>(fields.size());
    auto draw_border = [&](int y) {
        buffer.put(bounds.x, y, U'+');
        for (int column = 0; column < interior_width; ++column)
            buffer.put(bounds.x + 1 + column, y, U'-');
        buffer.put(bounds.x + bounds.width - 1, y, U'+');
    };
    draw_border(bounds.y);
    write_padded(buffer, bounds.x + 2, bounds.y, interior_width - 2, title_);

    const int header_y = bounds.y + 1;
    buffer.put(bounds.x, header_y, U'|');
    int x = bounds.x + 1;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const int width = base_width + (static_cast<int>(index) < remainder ? 1 : 0);
        const terminal::Style header_style{.bold = true, .underline = first_visible + index == selected_column_};
        write_padded(buffer, x, header_y, width, fields[index]->name, header_style);
        x += width;
        buffer.put(x++, header_y, U'|');
    }
    draw_border(bounds.y + 2);

    const int maximum_rows = bounds.height - 5;
    if (selected_result_row_) {
        if (*selected_result_row_ < first_visible_result_row_)
            first_visible_result_row_ = *selected_result_row_;
        else if (*selected_result_row_ >= first_visible_result_row_ + static_cast<std::size_t>(maximum_rows))
            first_visible_result_row_ = *selected_result_row_ - static_cast<std::size_t>(maximum_rows) + 1;
    }
    const auto selected = dataset_ != nullptr ? dataset_->current_row_index() : selected_result_row_;
    const auto& displayed_rows = dataset_ != nullptr ? dataset_->rows() : rows_->rows;
    for (int row_index = 0; row_index < maximum_rows; ++row_index) {
        const int y = bounds.y + 3 + row_index;
        buffer.put(bounds.x, y, U'|');
        x = bounds.x + 1;
        const auto source_index = dataset_ != nullptr ? static_cast<std::size_t>(row_index)
                                                      : first_visible_result_row_ + static_cast<std::size_t>(row_index);
        const auto row_exists = source_index < displayed_rows.size();
        for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
            const int width = base_width + (static_cast<int>(field_index) < remainder ? 1 : 0);
            const terminal::Style style{
                .underline = first_visible + field_index == selected_column_,
                .reverse = selected && *selected == source_index,
            };
            const auto text =
                row_exists ? display_value(displayed_rows[source_index].at(fields[field_index]->name)) : std::string{};
            write_padded(buffer, x, y, width, text, style);
            x += width;
            buffer.put(x++, y, U'|', style);
        }
    }
    const int footer_y = bounds.y + bounds.height - 2;
    buffer.put(bounds.x, footer_y, U'|');
    write_padded(buffer, bounds.x + 1, footer_y, interior_width, footer_);
    buffer.put(bounds.x + bounds.width - 1, footer_y, U'|');
    draw_border(bounds.y + bounds.height - 1);
}

} // namespace vulpes::ui
