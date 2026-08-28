#include "vulpes/ui/grid.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <charconv>
#include <variant>

namespace vulpes::ui {
namespace {

auto display_value(const db::Value& value, const core::LocaleFormatter* formatter) -> std::string {
    return std::visit(
        [formatter](const auto& item) -> std::string {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return {};
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return formatter == nullptr ? std::to_string(item) : formatter->number(item);
            else if constexpr (std::is_same_v<T, double>) {
                if (formatter != nullptr)
                    return formatter->number(item);
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

Grid::Grid(const model::Dataset& dataset, std::string title, std::string footer, const Theme& theme, GridText text,
           std::optional<core::LocaleFormatter> formatter)
    : dataset_{&dataset}, title_{std::move(title)}, footer_{std::move(footer)}, theme_{&theme},
      formatter_{std::move(formatter)}, text_{std::move(text)} {
}

Grid::Grid(const GridRows& rows, std::string title, std::string footer, const Theme& theme, GridText text,
           std::optional<core::LocaleFormatter> formatter)
    : dataset_{}, rows_{&rows}, title_{std::move(title)}, footer_{std::move(footer)}, theme_{&theme},
      formatter_{std::move(formatter)}, text_{std::move(text)} {
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

auto Grid::selected_column_width() const -> std::optional<int> {
    const auto* field = selected_field();
    if (field == nullptr)
        return std::nullopt;
    const auto width = rendered_column_widths_.find(field->name);
    return width == rendered_column_widths_.end() ? std::nullopt : std::optional{width->second};
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

auto Grid::resize_selected_column(int delta) -> bool {
    if (delta == 0)
        return false;
    const auto* field = selected_field();
    if (field == nullptr)
        return false;
    constexpr int minimum_width = 4;
    constexpr int maximum_width = 64;
    const auto rendered = rendered_column_widths_.find(field->name);
    const auto configured = column_width_overrides_.find(field->name);
    const auto current = configured != column_width_overrides_.end() ? configured->second
                         : rendered != rendered_column_widths_.end()
                             ? rendered->second
                             : std::clamp(terminal::text_width(field->name) + 2, minimum_width, maximum_width);
    const auto resized = std::clamp(current + delta, minimum_width, maximum_width);
    if (resized == current)
        return false;
    column_width_overrides_[field->name] = resized;
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

    const auto& current_theme = *theme_;
    const int interior_width = bounds.width - 2;
    constexpr int minimum_column_width = 4;
    auto visible_fields = [&](std::size_t first_visible) {
        std::vector<const db::FieldSchema*> fields;
        const auto maximum_count = static_cast<std::size_t>((interior_width + 1) / (minimum_column_width + 1));
        for (std::size_t index = first_visible; index < all_fields.size() && fields.size() < maximum_count; ++index)
            fields.push_back(all_fields[index]);
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
    if (content_width < static_cast<int>(fields.size()) * minimum_column_width)
        return;
    std::vector<int> column_widths(fields.size(), minimum_column_width);
    std::vector<int> preferred_widths;
    std::vector<bool> fixed_widths;
    preferred_widths.reserve(fields.size());
    fixed_widths.reserve(fields.size());
    const auto& displayed_rows = dataset_ != nullptr ? dataset_->rows() : rows_->rows;
    for (const auto* field : fields) {
        int preferred = terminal::text_width(field->name) + 2;
        for (const auto& row : displayed_rows)
            preferred = std::max(
                preferred,
                terminal::text_width(display_value(row.at(field->name), formatter_ ? &*formatter_ : nullptr)) + 2);
        const auto configured = column_width_overrides_.find(field->name);
        fixed_widths.push_back(configured != column_width_overrides_.end());
        preferred_widths.push_back(configured != column_width_overrides_.end()
                                       ? configured->second
                                       : std::clamp(preferred, minimum_column_width, 32));
    }
    int remaining = content_width - static_cast<int>(fields.size()) * minimum_column_width;
    for (bool changed = true; remaining > 0 && changed;) {
        changed = false;
        for (std::size_t index = 0; index < column_widths.size() && remaining > 0; ++index) {
            if (column_widths[index] >= preferred_widths[index])
                continue;
            ++column_widths[index];
            --remaining;
            changed = true;
        }
    }
    std::vector<std::size_t> flexible_columns;
    for (std::size_t index = 0; index < fixed_widths.size(); ++index)
        if (!fixed_widths[index])
            flexible_columns.push_back(index);
    if (flexible_columns.empty())
        flexible_columns.push_back(column_widths.size() - 1);
    for (std::size_t index = 0; remaining > 0; ++index, --remaining)
        ++column_widths[flexible_columns[index % flexible_columns.size()]];
    for (std::size_t index = 0; index < fields.size(); ++index)
        rendered_column_widths_[fields[index]->name] = column_widths[index];

    auto draw_border = [&](int y, char32_t left, char32_t junction, char32_t right) {
        buffer.put(bounds.x, y, left, current_theme.style(ThemeRole::border));
        int column = bounds.x + 1;
        for (std::size_t index = 0; index < column_widths.size(); ++index) {
            for (int offset = 0; offset < column_widths[index]; ++offset)
                buffer.put(column++, y, U'─', current_theme.style(ThemeRole::border));
            if (index + 1 < column_widths.size())
                buffer.put(column++, y, junction, current_theme.style(ThemeRole::border));
        }
        buffer.put(bounds.x + bounds.width - 1, y, right, current_theme.style(ThemeRole::border));
    };
    draw_border(bounds.y, U'┌', U'┬', U'┐');
    write_padded(buffer, bounds.x + 2, bounds.y, interior_width - 2, " " + title_ + " ",
                 current_theme.style(ThemeRole::title));

    const int header_y = bounds.y + 1;
    buffer.put(bounds.x, header_y, U'│', current_theme.style(ThemeRole::border));
    int x = bounds.x + 1;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const int width = column_widths[index];
        auto header_style = current_theme.style(ThemeRole::grid_header);
        header_style.underline = first_visible + index == selected_column_;
        write_padded(buffer, x, header_y, width, fields[index]->name, header_style);
        x += width;
        buffer.put(x++, header_y, U'│', current_theme.style(ThemeRole::border));
    }
    if (first_visible > 0)
        buffer.put(bounds.x, header_y, U'◀', current_theme.style(ThemeRole::border));
    if (first_visible + fields.size() < all_fields.size())
        buffer.put(bounds.x + bounds.width - 1, header_y, U'▶', current_theme.style(ThemeRole::border));
    draw_border(bounds.y + 2, U'├', U'┼', U'┤');

    const int maximum_rows = bounds.height - 5;
    if (dataset_ != nullptr && last_dataset_page_offset_ != dataset_->page_offset()) {
        first_visible_row_ = 0;
        last_dataset_page_offset_ = dataset_->page_offset();
    }
    const auto selected = dataset_ != nullptr ? dataset_->current_row_index() : selected_result_row_;
    if (selected) {
        if (*selected < first_visible_row_)
            first_visible_row_ = *selected;
        else if (*selected >= first_visible_row_ + static_cast<std::size_t>(maximum_rows))
            first_visible_row_ = *selected - static_cast<std::size_t>(maximum_rows) + 1;
    }
    const auto maximum_first = displayed_rows.size() > static_cast<std::size_t>(maximum_rows)
                                   ? displayed_rows.size() - static_cast<std::size_t>(maximum_rows)
                                   : 0;
    first_visible_row_ = std::min(first_visible_row_, maximum_first);
    for (int row_index = 0; row_index < maximum_rows; ++row_index) {
        const int y = bounds.y + 3 + row_index;
        buffer.put(bounds.x, y, U'│', current_theme.style(ThemeRole::border));
        x = bounds.x + 1;
        const auto source_index = first_visible_row_ + static_cast<std::size_t>(row_index);
        const auto row_exists = source_index < displayed_rows.size();
        for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
            const int width = column_widths[field_index];
            const bool selected_row = selected && *selected == source_index;
            const bool selected_cell = focused_ && selected_row && first_visible + field_index == selected_column_;
            const auto& style = current_theme.style(selected_cell  ? ThemeRole::grid_selected_cell
                                                    : selected_row ? ThemeRole::grid_selected_row
                                                                   : ThemeRole::grid_cell);
            const auto text = row_exists ? display_value(displayed_rows[source_index].at(fields[field_index]->name),
                                                         formatter_ ? &*formatter_ : nullptr)
                                         : std::string{};
            write_padded(buffer, x, y, width, text, style);
            x += width;
            buffer.put(x++, y, U'│', current_theme.style(ThemeRole::border));
        }
    }
    if (displayed_rows.empty()) {
        const auto message = "‹ " + text_.empty + " ›";
        const auto message_width = terminal::text_width(message);
        const auto message_x = bounds.x + 1 + std::max(0, (interior_width - message_width) / 2);
        static_cast<void>(buffer.write_utf8(message_x, bounds.y + 3 + maximum_rows / 2,
                                            terminal::truncate_utf8(message, interior_width),
                                            current_theme.style(ThemeRole::muted_text)));
    }

    const auto total_rows = dataset_ != nullptr ? dataset_->total_count() : displayed_rows.size();
    const auto absolute_selected = selected ? (dataset_ != nullptr ? dataset_->page_offset() : 0) + *selected : 0;
    if (total_rows > static_cast<std::size_t>(maximum_rows) && maximum_rows > 0) {
        for (int row = 0; row < maximum_rows; ++row)
            buffer.put(bounds.x + bounds.width - 1, bounds.y + 3 + row, U'░', current_theme.style(ThemeRole::border));
        const auto thumb =
            total_rows <= 1
                ? 0
                : static_cast<int>(absolute_selected * static_cast<std::size_t>(maximum_rows - 1) / (total_rows - 1));
        buffer.put(bounds.x + bounds.width - 1, bounds.y + 3 + thumb, U'█', current_theme.style(ThemeRole::border));
    }

    const int footer_y = bounds.y + bounds.height - 2;
    buffer.put(bounds.x, footer_y, U'│', current_theme.style(ThemeRole::border));
    write_padded(buffer, bounds.x + 1, footer_y, interior_width, footer_, current_theme.style(ThemeRole::grid_footer));
    auto position = selected
                        ? text_.row + " " + std::to_string(absolute_selected + 1) + "/" + std::to_string(total_rows)
                        : text_.rows + " 0";
    position +=
        "  " + text_.column + " " + std::to_string(selected_column_ + 1) + "/" + std::to_string(all_fields.size());
    const auto position_width = terminal::text_width(position);
    if (position_width < interior_width) {
        const auto position_x = bounds.x + bounds.width - 1 - position_width;
        if (position_x > bounds.x + 1)
            buffer.put(position_x - 1, footer_y, U' ', current_theme.style(ThemeRole::grid_footer));
        static_cast<void>(
            buffer.write_utf8(position_x, footer_y, position, current_theme.style(ThemeRole::grid_footer)));
    }
    buffer.put(bounds.x + bounds.width - 1, footer_y, U'│', current_theme.style(ThemeRole::border));
    draw_border(bounds.y + bounds.height - 1, U'└', U'┴', U'┘');
}

} // namespace vulpes::ui
