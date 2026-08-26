#include "vulpes/ui/grid.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <charconv>
#include <variant>

namespace vulpes::ui {
namespace {

auto display_value(const db::Value& value) -> std::string {
    return std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::monostate>) return {};
        else if constexpr (std::is_same_v<T, std::int64_t>) return std::to_string(item);
        else if constexpr (std::is_same_v<T, double>) {
            char output[64]{};
            const auto [end, error] = std::to_chars(std::begin(output), std::end(output), item);
            return error == std::errc{} ? std::string{output, end} : "?";
        } else if constexpr (std::is_same_v<T, std::string>) return item;
        else return "<blob " + std::to_string(item.size()) + " bytes>";
    }, value.storage());
}

void write_padded(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text, terminal::Style style = {}) {
    const auto clipped = terminal::truncate_utf8(text, width);
    const auto end = buffer.write_utf8(x, y, clipped, style);
    for (int column = end; column < x + width; ++column) buffer.put(column, y, U' ', style);
}

} // namespace

Grid::Grid(const model::Dataset& dataset, std::string title) : dataset_{&dataset}, title_{std::move(title)} {}

void Grid::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (bounds.width < 4 || bounds.height < 5 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height()) return;

    std::vector<const db::FieldSchema*> fields;
    for (const auto& field : dataset_->schema().fields) if (!field.hidden) fields.push_back(&field);
    if (fields.empty()) return;

    const int interior_width = bounds.width - 2;
    const int content_width = interior_width - static_cast<int>(fields.size()) + 1;
    if (content_width < static_cast<int>(fields.size())) return;
    const int base_width = content_width / static_cast<int>(fields.size());
    const int remainder = content_width % static_cast<int>(fields.size());
    auto draw_border = [&](int y) {
        buffer.put(bounds.x, y, U'+');
        for (int column = 0; column < interior_width; ++column) buffer.put(bounds.x + 1 + column, y, U'-');
        buffer.put(bounds.x + bounds.width - 1, y, U'+');
    };
    draw_border(bounds.y);
    write_padded(buffer, bounds.x + 2, bounds.y, interior_width - 2, title_);

    const int header_y = bounds.y + 1;
    buffer.put(bounds.x, header_y, U'|');
    int x = bounds.x + 1;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const int width = base_width + (static_cast<int>(index) < remainder ? 1 : 0);
        write_padded(buffer, x, header_y, width, fields[index]->name, terminal::Style{.bold = true});
        x += width;
        buffer.put(x++, header_y, U'|');
    }
    draw_border(bounds.y + 2);

    const auto selected = dataset_->current_row_index();
    const int maximum_rows = bounds.height - 4;
    for (int row_index = 0; row_index < maximum_rows; ++row_index) {
        const int y = bounds.y + 3 + row_index;
        buffer.put(bounds.x, y, U'|');
        x = bounds.x + 1;
        const auto row_exists = static_cast<std::size_t>(row_index) < dataset_->rows().size();
        const terminal::Style style{.reverse = selected && *selected == static_cast<std::size_t>(row_index)};
        for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
            const int width = base_width + (static_cast<int>(field_index) < remainder ? 1 : 0);
            const auto text = row_exists ? display_value(dataset_->rows()[static_cast<std::size_t>(row_index)].at(fields[field_index]->name)) : std::string{};
            write_padded(buffer, x, y, width, text, style);
            x += width;
            buffer.put(x++, y, U'|', style);
        }
    }
    draw_border(bounds.y + bounds.height - 1);
}

} // namespace vulpes::ui

