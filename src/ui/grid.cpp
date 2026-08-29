#include "vulpes/ui/grid.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <limits>
#include <set>
#include <utility>
#include <variant>

namespace vulpes::ui {
namespace {

auto raw_value(const db::Value& value) -> std::string {
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

auto display_value(const db::Value& value, const core::LocaleFormatter* formatter,
                   const appmeta::FieldMetadata* metadata) -> std::string {
    const auto automatic = [&] {
        if (formatter != nullptr && std::holds_alternative<std::int64_t>(value.storage()))
            return formatter->number(value.as_int());
        if (formatter != nullptr && std::holds_alternative<double>(value.storage()))
            return formatter->number(value.as_double());
        return raw_value(value);
    };
    if (metadata == nullptr || metadata->format == appmeta::FieldFormat::automatic)
        return automatic();
    try {
        switch (metadata->format) {
        case appmeta::FieldFormat::number:
            return automatic();
        case appmeta::FieldFormat::currency:
            if (formatter != nullptr && metadata->currency_code &&
                std::holds_alternative<std::int64_t>(value.storage())) {
                return formatter->currency(static_cast<double>(value.as_int()), *metadata->currency_code);
            }
            if (formatter != nullptr && metadata->currency_code && std::holds_alternative<double>(value.storage()))
                return formatter->currency(value.as_double(), *metadata->currency_code);
            return raw_value(value);
        case appmeta::FieldFormat::boolean:
            if (std::holds_alternative<std::int64_t>(value.storage()))
                return value.as_int() == 0 ? " " : "\xE2\x9C\x93";
            return raw_value(value);
        case appmeta::FieldFormat::date:
            return formatter != nullptr && std::holds_alternative<std::string>(value.storage())
                       ? formatter->iso_date(value.as_string())
                       : raw_value(value);
        case appmeta::FieldFormat::time:
            return formatter != nullptr && std::holds_alternative<std::string>(value.storage())
                       ? formatter->iso_time(value.as_string())
                       : raw_value(value);
        case appmeta::FieldFormat::date_time:
            if (formatter != nullptr && metadata->time_zone && std::holds_alternative<std::string>(value.storage())) {
                return core::LocaleFormatter{std::string{formatter->locale()}, *metadata->time_zone}.rfc3339(
                    value.as_string());
            }
            return raw_value(value);
        case appmeta::FieldFormat::text:
        case appmeta::FieldFormat::automatic:
            return raw_value(value);
        }
    } catch (const Error&) {
        return "! " + raw_value(value);
    }
    return raw_value(value);
}

void write_padded(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text,
                  terminal::Style style = {}) {
    const auto clipped = terminal::truncate_utf8(text, width);
    const auto end = buffer.write_utf8(x, y, clipped, style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

auto value_less(const db::Value& left, const db::Value& right) -> bool {
    if (left.storage().index() != right.storage().index())
        return left.storage().index() < right.storage().index();
    if (left.is_null())
        return false;
    if (std::holds_alternative<std::int64_t>(left.storage()))
        return left.as_int() < right.as_int();
    if (std::holds_alternative<double>(left.storage()))
        return left.as_double() < right.as_double();
    if (std::holds_alternative<std::string>(left.storage()))
        return left.as_string() < right.as_string();
    return std::ranges::lexicographical_compare(left.as_blob(), right.as_blob());
}

} // namespace

auto GridRows::from_sql_result(db::SqlResult result) -> GridRows {
    GridRows grid_rows;
    grid_rows.truncated = result.truncated;
    grid_rows.fields.reserve(result.columns.size());
    for (auto& column : result.columns)
        grid_rows.fields.push_back({.name = std::move(column)});
    grid_rows.rows = std::move(result.rows);
    return grid_rows;
}

struct Grid::DisplayColumn {
    const db::FieldSchema* field{};
    const appmeta::FieldMetadata* metadata{};
    const GridCalculatedColumn* calculation{};
    std::string id;
    std::string label;

    [[nodiscard]] auto read_only() const noexcept -> bool { return calculation != nullptr; }

    [[nodiscard]] auto value_for(const db::Row& row) const -> db::Value {
        return field != nullptr ? row.at(field->name) : calculation->value(row);
    }
};

Grid::Grid(const model::Dataset& dataset, std::string title, std::string footer, const Theme& theme, GridText text,
           std::optional<core::LocaleFormatter> formatter, std::optional<appmeta::TableMetadata> metadata,
           GridOptions options)
    : dataset_{&dataset}, title_{std::move(title)}, footer_{std::move(footer)}, theme_{&theme},
      formatter_{std::move(formatter)}, metadata_{std::move(metadata)}, options_{std::move(options)},
      text_{std::move(text)} {
    validate_options();
}

Grid::Grid(const GridRows& rows, std::string title, std::string footer, const Theme& theme, GridText text,
           std::optional<core::LocaleFormatter> formatter, std::optional<appmeta::TableMetadata> metadata,
           GridOptions options)
    : rows_{&rows}, title_{std::move(title)}, footer_{std::move(footer)}, theme_{&theme},
      formatter_{std::move(formatter)}, metadata_{std::move(metadata)}, options_{std::move(options)},
      text_{std::move(text)} {
    validate_options();
    if (!rows.rows.empty())
        selected_result_row_ = 0;
}

Grid::Grid(GridRows& rows, std::string title, std::string footer, const Theme& theme, GridText text,
           std::optional<core::LocaleFormatter> formatter, std::optional<appmeta::TableMetadata> metadata,
           GridOptions options)
    : rows_{&rows}, mutable_rows_{&rows}, title_{std::move(title)}, footer_{std::move(footer)}, theme_{&theme},
      formatter_{std::move(formatter)}, metadata_{std::move(metadata)}, options_{std::move(options)},
      text_{std::move(text)} {
    validate_options();
    if (!rows.rows.empty())
        selected_result_row_ = 0;
}

auto Grid::display_columns() const -> std::vector<DisplayColumn> {
    std::vector<DisplayColumn> columns;
    const auto& schema_fields = dataset_ != nullptr ? dataset_->schema().fields : rows_->fields;
    for (const auto& field : schema_fields) {
        const auto* metadata = metadata_ == std::nullopt ? nullptr : metadata_->field(field.name);
        if (!field.hidden && (metadata == nullptr || metadata->visible != false)) {
            columns.push_back({.field = &field,
                               .metadata = metadata,
                               .id = field.name,
                               .label = metadata != nullptr && metadata->label ? *metadata->label : field.name});
        }
    }
    std::ranges::stable_sort(columns, [](const auto& left, const auto& right) {
        const auto left_order = left.metadata == nullptr ? std::nullopt : left.metadata->order;
        const auto right_order = right.metadata == nullptr ? std::nullopt : right.metadata->order;
        return left_order.value_or(std::numeric_limits<std::size_t>::max()) <
               right_order.value_or(std::numeric_limits<std::size_t>::max());
    });
    for (const auto& calculated : options_.calculated_columns)
        columns.push_back({.calculation = &calculated, .id = calculated.id, .label = calculated.label});
    return columns;
}

auto Grid::selected_field() const -> const db::FieldSchema* {
    const auto columns = display_columns();
    return selected_column_ < columns.size() ? columns[selected_column_].field : nullptr;
}

auto Grid::selected_column_is_read_only() const -> bool {
    const auto columns = display_columns();
    return selected_column_ >= columns.size() || columns[selected_column_].read_only();
}

auto Grid::selected_column_width() const -> std::optional<int> {
    const auto columns = display_columns();
    if (selected_column_ >= columns.size())
        return std::nullopt;
    const auto width = rendered_column_widths_.find(columns[selected_column_].id);
    return width == rendered_column_widths_.end() ? std::nullopt : std::optional{width->second};
}

auto Grid::move_left() -> bool {
    if (selected_column_ == 0)
        return false;
    --selected_column_;
    const auto frozen = std::min(options_.frozen_columns, display_columns().size());
    first_visible_column_ = selected_column_ < frozen ? frozen : std::min(first_visible_column_, selected_column_);
    return true;
}

auto Grid::move_right() -> bool {
    const auto column_count = display_columns().size();
    if (selected_column_ + 1 >= column_count)
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
    const auto columns = display_columns();
    if (selected_column_ >= columns.size())
        return false;
    constexpr int minimum_width = 4;
    constexpr int maximum_width = 64;
    const auto& column = columns[selected_column_];
    const auto rendered = rendered_column_widths_.find(column.id);
    const auto configured = column_width_overrides_.find(column.id);
    const auto current = configured != column_width_overrides_.end() ? configured->second
                         : rendered != rendered_column_widths_.end()
                             ? rendered->second
                             : std::clamp(terminal::text_width(column.label) + 2, minimum_width, maximum_width);
    const auto resized = std::clamp(current + delta, minimum_width, maximum_width);
    if (resized == current)
        return false;
    column_width_overrides_[column.id] = resized;
    return true;
}

auto Grid::sort_selected() -> bool {
    const auto* field = selected_field();
    if (field == nullptr || mutable_rows_ == nullptr)
        return false;
    bool ascending = true;
    if (result_sort_ && result_sort_->first == field->name)
        ascending = !result_sort_->second;
    result_sort_ = std::pair{field->name, ascending};
    std::ranges::stable_sort(mutable_rows_->rows, [&](const auto& left, const auto& right) {
        return ascending ? value_less(left.at(field->name), right.at(field->name))
                         : value_less(right.at(field->name), left.at(field->name));
    });
    if (!mutable_rows_->rows.empty())
        selected_result_row_ = 0;
    first_visible_row_ = 0;
    aggregate_dataset_revision_ = static_cast<std::size_t>(-1);
    return true;
}

void Grid::set_options(GridOptions options) {
    auto previous = std::move(options_);
    options_ = std::move(options);
    try {
        validate_options();
    } catch (...) {
        options_ = std::move(previous);
        throw;
    }
    const auto column_count = display_columns().size();
    selected_column_ = column_count == 0 ? 0 : std::min(selected_column_, column_count - 1);
    first_visible_column_ = std::min(first_visible_column_, column_count);
    column_width_overrides_.clear();
    rendered_column_widths_.clear();
    aggregate_results_.clear();
    aggregate_dataset_revision_ = static_cast<std::size_t>(-1);
}

auto Grid::aggregation_definitions() const -> std::vector<model::AggregateDefinition> {
    std::vector<model::AggregateDefinition> definitions;
    definitions.reserve(options_.aggregations.size());
    for (const auto& aggregation : options_.aggregations)
        definitions.push_back(aggregation.definition);
    return definitions;
}

auto Grid::aggregation_results() const -> const std::vector<model::AggregateResult>& {
    if (options_.aggregations.empty()) {
        aggregate_results_.clear();
        return aggregate_results_;
    }
    if (dataset_ == nullptr) {
        aggregate_results_ = calculate_row_aggregates();
        return aggregate_results_;
    }
    if (aggregate_dataset_revision_ != dataset_->query_revision()) {
        aggregate_results_ = dataset_->aggregate(aggregation_definitions());
        aggregate_dataset_revision_ = dataset_->query_revision();
    }
    return aggregate_results_;
}

auto Grid::calculate_row_aggregates() const -> std::vector<model::AggregateResult> {
    const auto definitions = aggregation_definitions();
    std::vector<model::AggregateResult> results;
    results.reserve(definitions.size());
    for (const auto& definition : definitions) {
        std::size_t count{};
        bool saw_double{};
        bool saw_number{};
        std::int64_t integer_sum{};
        double real_sum{};
        std::optional<db::Value> extremum;
        for (const auto& row : rows_->rows) {
            const auto value = definition.field ? row.at(*definition.field) : db::Value{1};
            if (definition.function == model::AggregateFunction::count) {
                if (!definition.field || !value.is_null())
                    ++count;
                continue;
            }
            if (value.is_null())
                continue;
            if (definition.function == model::AggregateFunction::sum ||
                definition.function == model::AggregateFunction::average) {
                if (std::holds_alternative<std::int64_t>(value.storage())) {
                    saw_number = true;
                    const auto number = value.as_int();
                    if (!saw_double &&
                        ((number > 0 && integer_sum > std::numeric_limits<std::int64_t>::max() - number) ||
                         (number < 0 && integer_sum < std::numeric_limits<std::int64_t>::min() - number))) {
                        saw_double = true;
                    }
                    if (!saw_double)
                        integer_sum += number;
                    real_sum += static_cast<double>(number);
                    ++count;
                } else if (std::holds_alternative<double>(value.storage())) {
                    saw_number = true;
                    saw_double = true;
                    real_sum += value.as_double();
                    ++count;
                }
                continue;
            }
            if (!extremum ||
                (definition.function == model::AggregateFunction::minimum && value_less(value, *extremum)) ||
                (definition.function == model::AggregateFunction::maximum && value_less(*extremum, value))) {
                extremum = value;
            }
        }
        db::Value value;
        switch (definition.function) {
        case model::AggregateFunction::count:
            value = static_cast<std::int64_t>(count);
            break;
        case model::AggregateFunction::sum:
            value = !saw_number ? db::Value{} : saw_double ? db::Value{real_sum} : db::Value{integer_sum};
            break;
        case model::AggregateFunction::average:
            value = count == 0 ? db::Value{} : db::Value{real_sum / static_cast<double>(count)};
            break;
        case model::AggregateFunction::minimum:
        case model::AggregateFunction::maximum:
            value = extremum.value_or(db::Value{});
            break;
        }
        results.push_back({.definition = definition, .value = std::move(value)});
    }
    return results;
}

void Grid::validate_options() const {
    std::set<std::string, std::less<>> identifiers;
    const auto& schema_fields = dataset_ != nullptr ? dataset_->schema().fields : rows_->fields;
    for (const auto& field : schema_fields)
        identifiers.insert(field.name);
    const auto source_identifiers = identifiers;
    for (const auto& column : options_.calculated_columns) {
        if (column.id.empty() || column.id.size() > 128 || column.label.empty() || !column.value ||
            !identifiers.insert(column.id).second) {
            throw Error{ErrorCategory::validation,
                        "grid calculated columns require unique identifiers, labels, and values"};
        }
    }
    for (const auto& aggregation : options_.aggregations) {
        if (aggregation.label.empty())
            throw Error{ErrorCategory::validation, "grid aggregations require a presentation label"};
        if (aggregation.definition.function != model::AggregateFunction::count && !aggregation.definition.field) {
            throw Error{ErrorCategory::validation, "this grid aggregation requires a source field"};
        }
        if (aggregation.definition.field && !source_identifiers.contains(*aggregation.definition.field)) {
            throw Error{ErrorCategory::validation,
                        "grid aggregation references an unknown source field: " + *aggregation.definition.field};
        }
    }
}

void Grid::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    if (bounds.width < 4 || bounds.height < 6 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height()) {
        return;
    }

    const auto all_columns = display_columns();
    if (all_columns.empty())
        return;

    const auto& current_theme = *theme_;
    const int interior_width = bounds.width - 2;
    constexpr int minimum_column_width = 4;
    const auto maximum_columns = static_cast<std::size_t>((interior_width + 1) / (minimum_column_width + 1));
    if (maximum_columns == 0)
        return;
    const auto configured_frozen = std::min(options_.frozen_columns, all_columns.size());
    const auto visible_frozen = std::min(configured_frozen, maximum_columns);
    auto first_visible = std::max(configured_frozen, std::min(first_visible_column_, all_columns.size() - 1));
    auto visible_columns = [&] {
        std::vector<DisplayColumn> columns;
        columns.reserve(maximum_columns);
        for (std::size_t index = 0; index < visible_frozen; ++index)
            columns.push_back(all_columns[index]);
        for (std::size_t index = first_visible; index < all_columns.size() && columns.size() < maximum_columns; ++index)
            columns.push_back(all_columns[index]);
        return columns;
    };
    auto columns = visible_columns();
    auto scrolling_count = columns.size() - visible_frozen;
    while (selected_column_ >= configured_frozen &&
           (selected_column_ < first_visible || selected_column_ >= first_visible + scrolling_count) &&
           first_visible < selected_column_) {
        ++first_visible;
        columns = visible_columns();
        scrolling_count = columns.size() - visible_frozen;
    }
    first_visible_column_ = first_visible;
    if (columns.empty())
        return;

    const int content_width = interior_width - static_cast<int>(columns.size()) + 1;
    if (content_width < static_cast<int>(columns.size()) * minimum_column_width)
        return;
    std::vector<int> column_widths(columns.size(), minimum_column_width);
    std::vector<int> preferred_widths;
    std::vector<bool> fixed_widths;
    preferred_widths.reserve(columns.size());
    fixed_widths.reserve(columns.size());
    const auto& displayed_rows = dataset_ != nullptr ? dataset_->rows() : rows_->rows;
    for (const auto& column : columns) {
        int preferred = terminal::text_width(column.label) + 2;
        for (const auto& row : displayed_rows) {
            try {
                preferred = std::max(preferred,
                                     terminal::text_width(display_value(
                                         column.value_for(row), formatter_ ? &*formatter_ : nullptr, column.metadata)) +
                                         2);
            } catch (const std::exception&) {
                preferred = std::max(preferred, 9);
            }
        }
        const auto configured = column_width_overrides_.find(column.id);
        fixed_widths.push_back(configured != column_width_overrides_.end());
        preferred_widths.push_back(configured != column_width_overrides_.end()
                                       ? configured->second
                                       : std::clamp(preferred, minimum_column_width, 32));
    }
    int remaining = content_width - static_cast<int>(columns.size()) * minimum_column_width;
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
    for (std::size_t index = 0; index < fixed_widths.size(); ++index) {
        if (!fixed_widths[index])
            flexible_columns.push_back(index);
    }
    if (flexible_columns.empty())
        flexible_columns.push_back(column_widths.size() - 1);
    for (std::size_t index = 0; remaining > 0; ++index, --remaining)
        ++column_widths[flexible_columns[index % flexible_columns.size()]];
    for (std::size_t index = 0; index < columns.size(); ++index)
        rendered_column_widths_[columns[index].id] = column_widths[index];

    auto draw_border = [&](int y, char32_t left, char32_t junction, char32_t right) {
        buffer.put(bounds.x, y, left, current_theme.style(ThemeRole::border));
        int column = bounds.x + 1;
        for (std::size_t index = 0; index < column_widths.size(); ++index) {
            for (int offset = 0; offset < column_widths[index]; ++offset)
                buffer.put(column++, y, U'\u2500', current_theme.style(ThemeRole::border));
            if (index + 1 < column_widths.size())
                buffer.put(column++, y, junction, current_theme.style(ThemeRole::border));
        }
        buffer.put(bounds.x + bounds.width - 1, y, right, current_theme.style(ThemeRole::border));
    };
    draw_border(bounds.y, U'\u250c', U'\u252c', U'\u2510');
    write_padded(buffer, bounds.x + 2, bounds.y, interior_width - 2, " " + title_ + " ",
                 current_theme.style(ThemeRole::title));

    const int header_y = bounds.y + 1;
    buffer.put(bounds.x, header_y, U'\u2502', current_theme.style(ThemeRole::border));
    int x = bounds.x + 1;
    for (std::size_t index = 0; index < columns.size(); ++index) {
        const int width = column_widths[index];
        auto header_style = current_theme.style(ThemeRole::grid_header);
        const auto absolute_column = index < visible_frozen ? index : first_visible + index - visible_frozen;
        header_style.underline = absolute_column == selected_column_;
        write_padded(buffer, x, header_y, width, columns[index].label, header_style);
        x += width;
        buffer.put(x++, header_y, U'\u2502', current_theme.style(ThemeRole::border));
    }
    if (first_visible > configured_frozen)
        buffer.put(bounds.x, header_y, U'\u25c0', current_theme.style(ThemeRole::border));
    if (first_visible + scrolling_count < all_columns.size())
        buffer.put(bounds.x + bounds.width - 1, header_y, U'\u25b6', current_theme.style(ThemeRole::border));
    draw_border(bounds.y + 2, U'\u251c', U'\u253c', U'\u2524');

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
        buffer.put(bounds.x, y, U'\u2502', current_theme.style(ThemeRole::border));
        x = bounds.x + 1;
        const auto source_index = first_visible_row_ + static_cast<std::size_t>(row_index);
        const auto row_exists = source_index < displayed_rows.size();
        for (std::size_t column_index = 0; column_index < columns.size(); ++column_index) {
            const int width = column_widths[column_index];
            const bool selected_row = selected && *selected == source_index;
            const auto absolute_column =
                column_index < visible_frozen ? column_index : first_visible + column_index - visible_frozen;
            const bool selected_cell = focused_ && selected_row && absolute_column == selected_column_;
            const auto& style = current_theme.style(selected_cell  ? ThemeRole::grid_selected_cell
                                                    : selected_row ? ThemeRole::grid_selected_row
                                                                   : ThemeRole::grid_cell);
            std::string text;
            if (row_exists) {
                try {
                    text = display_value(columns[column_index].value_for(displayed_rows[source_index]),
                                         formatter_ ? &*formatter_ : nullptr, columns[column_index].metadata);
                } catch (const std::exception&) {
                    text = "<error>";
                }
            }
            write_padded(buffer, x, y, width, text, style);
            x += width;
            buffer.put(x++, y, U'\u2502', current_theme.style(ThemeRole::border));
        }
    }
    if (displayed_rows.empty()) {
        const auto message = "< " + text_.empty + " >";
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
            buffer.put(bounds.x + bounds.width - 1, bounds.y + 3 + row, U'\u2591',
                       current_theme.style(ThemeRole::border));
        const auto thumb =
            total_rows <= 1
                ? 0
                : static_cast<int>(absolute_selected * static_cast<std::size_t>(maximum_rows - 1) / (total_rows - 1));
        buffer.put(bounds.x + bounds.width - 1, bounds.y + 3 + thumb, U'\u2588',
                   current_theme.style(ThemeRole::border));
    }

    const int footer_y = bounds.y + bounds.height - 2;
    buffer.put(bounds.x, footer_y, U'\u2502', current_theme.style(ThemeRole::border));
    auto footer = footer_;
    const auto& aggregates = aggregation_results();
    for (std::size_t index = 0; index < aggregates.size(); ++index) {
        if (!footer.empty())
            footer += "  ";
        footer += options_.aggregations[index].label + ": " +
                  display_value(aggregates[index].value, formatter_ ? &*formatter_ : nullptr, nullptr);
    }
    write_padded(buffer, bounds.x + 1, footer_y, interior_width, footer, current_theme.style(ThemeRole::grid_footer));
    auto position = selected
                        ? text_.row + " " + std::to_string(absolute_selected + 1) + "/" + std::to_string(total_rows)
                        : text_.rows + " 0";
    position +=
        "  " + text_.column + " " + std::to_string(selected_column_ + 1) + "/" + std::to_string(all_columns.size());
    const auto position_width = terminal::text_width(position);
    if (position_width < interior_width) {
        const auto position_x = bounds.x + bounds.width - 1 - position_width;
        if (position_x > bounds.x + 1)
            buffer.put(position_x - 1, footer_y, U' ', current_theme.style(ThemeRole::grid_footer));
        static_cast<void>(
            buffer.write_utf8(position_x, footer_y, position, current_theme.style(ThemeRole::grid_footer)));
    }
    buffer.put(bounds.x + bounds.width - 1, footer_y, U'\u2502', current_theme.style(ThemeRole::border));
    draw_border(bounds.y + bounds.height - 1, U'\u2514', U'\u2534', U'\u2518');
}

} // namespace vulpes::ui
