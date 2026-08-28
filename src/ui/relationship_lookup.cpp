#include "vulpes/ui/relationship_lookup.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/unicode.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <type_traits>
#include <variant>

namespace vulpes::ui {
namespace {

void write_padded(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text,
                  terminal::Style style = {}) {
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

[[nodiscard]] auto display_value(const db::Value& value) -> std::string {
    return std::visit(
        [](const auto& item) -> std::string {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::monostate>)
                return {};
            else if constexpr (std::is_same_v<Value, std::int64_t> || std::is_same_v<Value, double>)
                return std::to_string(item);
            else if constexpr (std::is_same_v<Value, std::string>)
                return item;
            else
                return "<blob " + std::to_string(item.size()) + " bytes>";
        },
        value.storage());
}

} // namespace

RelationshipLookup::RelationshipLookup(model::Dataset& dataset, std::string field, model::LookupQuery query,
                                       bool allow_drill_down, std::string title, std::string search_label,
                                       std::string instructions, const Theme& theme, core::Clipboard* clipboard)
    : dataset_{&dataset}, field_{std::move(field)}, query_{std::move(query)}, allow_drill_down_{allow_drill_down},
      title_{std::move(title)}, search_label_{std::move(search_label)}, instructions_{std::move(instructions)},
      theme_{&theme}, clipboard_{clipboard} {
    query_.search.clear();
    refresh();
}

auto RelationshipLookup::handle(const terminal::InputEvent& event) -> RelationshipLookupResult {
    if (std::holds_alternative<terminal::ResizeEvent>(event))
        return RelationshipLookupResult::redraw;
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key != nullptr && key->key == terminal::Key::escape)
        return RelationshipLookupResult::cancelled;
    if (key != nullptr && key->key == terminal::Key::enter)
        return selected_option() == nullptr ? RelationshipLookupResult::unchanged : RelationshipLookupResult::selected;
    if (key != nullptr && key->key == terminal::Key::f2 && allow_drill_down_)
        return selected_option() == nullptr ? RelationshipLookupResult::unchanged
                                            : RelationshipLookupResult::drill_down;
    if (key != nullptr && key->key == terminal::Key::up) {
        move_selection(-1);
        return RelationshipLookupResult::redraw;
    }
    if (key != nullptr && key->key == terminal::Key::down) {
        move_selection(1);
        return RelationshipLookupResult::redraw;
    }
    if (key != nullptr && key->key == terminal::Key::page_up) {
        move_selection(-10);
        return RelationshipLookupResult::redraw;
    }
    if (key != nullptr && key->key == terminal::Key::page_down) {
        move_selection(10);
        return RelationshipLookupResult::redraw;
    }

    const auto result = search_.handle(event, clipboard_);
    if (result == LineEditResult::changed) {
        query_.search = std::string{search_.text()};
        try {
            refresh();
            error_.clear();
        } catch (const Error& error) {
            options_.clear();
            selected_ = 0;
            error_ = error.what();
        }
        return RelationshipLookupResult::redraw;
    }
    return result == LineEditResult::unchanged ? RelationshipLookupResult::unchanged : RelationshipLookupResult::redraw;
}

void RelationshipLookup::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 28, 8))
        return;
    WindowFrame::render(buffer, bounds, title_, window_frame_appearance(*theme_, true));
    const int interior = bounds.width - 2;
    const int label_width = std::min(12, terminal::text_width(search_label_) + 1);
    write_padded(buffer, bounds.x + 1, bounds.y + 1, label_width, search_label_, theme_->style(ThemeRole::text));
    search_.render(buffer, {bounds.x + 1 + label_width, bounds.y + 1, interior - label_width, 1},
                   theme_->style(ThemeRole::input_focus), true);
    for (int column = 1; column < bounds.width - 1; ++column)
        buffer.put(bounds.x + column, bounds.y + 2, U'─', theme_->style(ThemeRole::border));

    const int visible_rows = bounds.height - 5;
    const auto first = selected_ < static_cast<std::size_t>(visible_rows)
                           ? std::size_t{0}
                           : selected_ - static_cast<std::size_t>(visible_rows) + 1;
    for (int row = 0; row < visible_rows; ++row) {
        const auto index = first + static_cast<std::size_t>(row);
        const bool selected = index < options_.size() && index == selected_;
        const auto style = theme_->style(selected ? ThemeRole::grid_selected_row : ThemeRole::grid_cell);
        const auto text = index < options_.size() ? (selected ? "› " : "  ") + options_[index].label : std::string{};
        write_padded(buffer, bounds.x + 1, bounds.y + 3 + row, interior, text, style);
    }
    write_padded(buffer, bounds.x + 1, bounds.y + bounds.height - 2, interior, error_.empty() ? instructions_ : error_,
                 theme_->style(error_.empty() ? ThemeRole::muted_text : ThemeRole::error));
}

auto RelationshipLookup::selected_option() const noexcept -> const model::LookupOption* {
    return selected_ < options_.size() ? &options_[selected_] : nullptr;
}

void RelationshipLookup::refresh() {
    options_ = dataset_->lookup_options(field_, query_);
    selected_ = 0;
}

void RelationshipLookup::move_selection(int direction) {
    if (options_.empty())
        return;
    const auto current = static_cast<std::ptrdiff_t>(selected_);
    const auto last = static_cast<std::ptrdiff_t>(options_.size() - 1);
    selected_ = static_cast<std::size_t>(std::clamp(current + direction, std::ptrdiff_t{0}, last));
}

RelatedRecordView::RelatedRecordView(model::RelatedRecord record, std::string title, std::string instructions,
                                     const Theme& theme, const appmeta::TableMetadata* metadata)
    : title_{std::move(title)}, instructions_{std::move(instructions)}, theme_{&theme} {
    std::vector<std::size_t> order(record.schema.fields.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, [&](std::size_t left, std::size_t right) {
        const auto* left_metadata = metadata == nullptr ? nullptr : metadata->field(record.schema.fields[left].name);
        const auto* right_metadata = metadata == nullptr ? nullptr : metadata->field(record.schema.fields[right].name);
        const auto left_order = left_metadata == nullptr ? std::nullopt : left_metadata->order;
        const auto right_order = right_metadata == nullptr ? std::nullopt : right_metadata->order;
        return left_order.value_or(std::numeric_limits<std::size_t>::max()) <
               right_order.value_or(std::numeric_limits<std::size_t>::max());
    });
    for (const auto index : order) {
        const auto& field = record.schema.fields[index];
        if (field.hidden)
            continue;
        const auto* field_metadata = metadata == nullptr ? nullptr : metadata->field(field.name);
        if (field_metadata != nullptr && field_metadata->visible == false)
            continue;
        fields_.emplace_back(field_metadata != nullptr && field_metadata->label ? *field_metadata->label : field.name,
                             display_value(record.row.at(field.name)));
    }
}

auto RelatedRecordView::handle(const terminal::InputEvent& event) -> RelatedRecordResult {
    if (std::holds_alternative<terminal::ResizeEvent>(event))
        return RelatedRecordResult::redraw;
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key == nullptr)
        return RelatedRecordResult::unchanged;
    if (key->key == terminal::Key::escape)
        return RelatedRecordResult::cancelled;
    if (fields_.empty())
        return RelatedRecordResult::unchanged;
    if (key->key == terminal::Key::up)
        selected_ = selected_ == 0 ? 0 : selected_ - 1;
    else if (key->key == terminal::Key::down)
        selected_ = std::min(selected_ + 1, fields_.size() - 1);
    else
        return RelatedRecordResult::unchanged;
    return RelatedRecordResult::redraw;
}

void RelatedRecordView::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 24, 6))
        return;
    WindowFrame::render(buffer, bounds, title_, window_frame_appearance(*theme_, true));
    const int interior = bounds.width - 2;
    const int label_width = std::min(18, interior / 2);
    const int visible_rows = bounds.height - 3;
    const auto first = selected_ < static_cast<std::size_t>(visible_rows)
                           ? std::size_t{0}
                           : selected_ - static_cast<std::size_t>(visible_rows) + 1;
    for (int row = 0; row < visible_rows; ++row) {
        const auto index = first + static_cast<std::size_t>(row);
        if (index >= fields_.size()) {
            write_padded(buffer, bounds.x + 1, bounds.y + 1 + row, interior, {}, theme_->style(ThemeRole::text));
            continue;
        }
        const auto role = index == selected_ ? ThemeRole::grid_selected_row : ThemeRole::text;
        write_padded(buffer, bounds.x + 1, bounds.y + 1 + row, label_width, fields_[index].first + ':',
                     theme_->style(role));
        write_padded(buffer, bounds.x + 1 + label_width, bounds.y + 1 + row, interior - label_width,
                     fields_[index].second, theme_->style(role));
    }
    write_padded(buffer, bounds.x + 1, bounds.y + bounds.height - 2, interior, instructions_,
                 theme_->style(ThemeRole::muted_text));
}

} // namespace vulpes::ui
