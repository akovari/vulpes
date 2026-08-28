#include "vulpes/ui/schema_document.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>

namespace vulpes::ui {
namespace {

void write_padded(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text,
                  terminal::Style style = {}) {
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

auto describe_field(const db::FieldSchema& field, const core::Localizer& messages) -> std::string {
    auto text = field.name + (field.declared_type.empty() ? std::string{} : " : " + field.declared_type);
    if (!field.nullable)
        text += " [" + messages.translate("schema.not_null") + "]";
    if (field.primary_key)
        text += " [" + messages.translate("schema.primary_key") + "]";
    if (field.unique)
        text += " [" + messages.translate("schema.unique") + "]";
    if (field.generated)
        text += " [" + messages.translate("schema.generated") + "]";
    return text;
}

} // namespace

SchemaDocument::SchemaDocument(db::TableSchema table, const core::Localizer& messages, const Theme& theme)
    : table_{std::move(table)}, title_{messages.translate("schema.title", {{"name", table_.name}})},
      footer_{messages.translate("schema.footer")}, theme_{&theme} {
    lines_.reserve(table_.fields.size());
    for (const auto& field : table_.fields)
        lines_.push_back(describe_field(field, messages));
}

auto SchemaDocument::handle(core::ActionId action, const terminal::InputEvent&) -> DocumentResult {
    switch (action) {
    case core::ActionId::application_back:
    case core::ActionId::application_quit:
        return DocumentResult::close;
    case core::ActionId::dataset_previous:
        if (selected_line_ == 0)
            return DocumentResult::unchanged;
        --selected_line_;
        first_visible_line_ = std::min(first_visible_line_, selected_line_);
        return DocumentResult::redraw;
    case core::ActionId::dataset_next:
        if (selected_line_ + 1 >= lines_.size())
            return DocumentResult::unchanged;
        ++selected_line_;
        return DocumentResult::redraw;
    case core::ActionId::dataset_first:
        if (selected_line_ == 0)
            return DocumentResult::unchanged;
        selected_line_ = 0;
        first_visible_line_ = 0;
        return DocumentResult::redraw;
    case core::ActionId::dataset_last:
        if (lines_.empty() || selected_line_ + 1 == lines_.size())
            return DocumentResult::unchanged;
        selected_line_ = lines_.size() - 1;
        return DocumentResult::redraw;
    default:
        return DocumentResult::unchanged;
    }
}

void SchemaDocument::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    if (bounds.width < 20 || bounds.height < 5 || bounds.x < 0 || bounds.y < 0 ||
        bounds.x + bounds.width > buffer.width() || bounds.y + bounds.height > buffer.height()) {
        return;
    }

    write_padded(buffer, bounds.x, bounds.y, bounds.width, title_, theme_->style(ThemeRole::title));
    const int content_height = bounds.height - 2;
    if (selected_line_ >= first_visible_line_ + static_cast<std::size_t>(content_height))
        first_visible_line_ = selected_line_ - static_cast<std::size_t>(content_height) + 1;
    for (int row = 0; row < content_height; ++row) {
        const auto index = first_visible_line_ + static_cast<std::size_t>(row);
        if (index >= lines_.size()) {
            write_padded(buffer, bounds.x, bounds.y + 1 + row, bounds.width, "", theme_->style(ThemeRole::text));
            continue;
        }
        write_padded(buffer, bounds.x, bounds.y + 1 + row, bounds.width, lines_[index],
                     theme_->style(index == selected_line_ ? ThemeRole::selection : ThemeRole::text));
    }
    write_padded(buffer, bounds.x, bounds.y + bounds.height - 1, bounds.width, footer_,
                 theme_->style(ThemeRole::grid_footer));
}

} // namespace vulpes::ui
