#include "vulpes/ui/sql_document.hpp"

#include "vulpes/core/error.hpp"

#include <algorithm>
#include <cctype>

namespace vulpes::ui {
namespace {

auto trim_ascii(std::string_view text) -> std::string_view {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        text.remove_suffix(1);
    return text;
}

} // namespace

SqlDocument::SqlDocument(db::Database& database, const core::Localizer& messages, const Theme& theme,
                         core::Clipboard* clipboard)
    : database_{&database}, messages_{&messages}, theme_{&theme},
      console_{messages.translate("sql.title"), messages.translate("sql.instructions"), theme, clipboard} {
}

auto SqlDocument::is_dirty() const noexcept -> bool {
    return !trim_ascii(console_.script()).empty() && console_.script() != last_executed_script_;
}

void SqlDocument::execute() {
    if (trim_ascii(console_.script()).empty())
        throw Error{ErrorCategory::validation, messages_->translate("sql.empty_error")};
    result_grid_.reset();
    result_focused_ = false;
    auto result = database_->run_sql(console_.script());
    last_executed_script_ = console_.script();
    const auto rows = result.rows.size();
    const auto changes = result.changes;
    const auto truncated = result.truncated ? messages_->translate("sql.truncated") : std::string{};
    result_rows_ = GridRows::from_sql_result(std::move(result));
    if (!result_rows_->fields.empty())
        result_grid_.emplace(*result_rows_, messages_->translate("sql.results"),
                             messages_->translate("sql.result_footer"), *theme_,
                             GridText{.empty = messages_->translate("grid.empty"),
                                      .row = messages_->translate("grid.row"),
                                      .rows = messages_->translate("grid.rows"),
                                      .column = messages_->translate("grid.column")},
                             core::LocaleFormatter{std::string{messages_->locale()}});
    console_.set_status(messages_->translate(
        "sql.status",
        {{"rows", std::to_string(rows)}, {"changes", std::to_string(changes)}, {"truncated", truncated}}));
}

auto SqlDocument::handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult {
    if (result_grid_ && result_pane_visible_ && action == core::ActionId::document_switch_pane) {
        result_focused_ = !result_focused_;
        return DocumentResult::redraw;
    }
    if (result_grid_ && result_focused_) {
        if (action == core::ActionId::application_back) {
            result_focused_ = false;
            return DocumentResult::redraw;
        }
        const auto* key = std::get_if<terminal::KeyEvent>(&event);
        if (key != nullptr && key->key == terminal::Key::f8)
            result_focused_ = false;
        else {
            if (action == core::ActionId::dataset_previous && result_grid_->move_previous_row())
                return DocumentResult::redraw;
            if (action == core::ActionId::dataset_next && result_grid_->move_next_row())
                return DocumentResult::redraw;
            if (action == core::ActionId::grid_previous_column && result_grid_->move_left())
                return DocumentResult::redraw;
            if (action == core::ActionId::grid_next_column && result_grid_->move_right())
                return DocumentResult::redraw;
            if (action == core::ActionId::grid_narrow_column && result_grid_->resize_selected_column(-1))
                return DocumentResult::redraw;
            if (action == core::ActionId::grid_widen_column && result_grid_->resize_selected_column(1))
                return DocumentResult::redraw;
            if (action == core::ActionId::dataset_sort && result_grid_->sort_selected())
                return DocumentResult::redraw;
            return DocumentResult::unchanged;
        }
    }
    const auto result = console_.handle(event);
    if (result == SqlConsoleResult::cancelled)
        return DocumentResult::close;
    if (result != SqlConsoleResult::execute)
        return result == SqlConsoleResult::unchanged ? DocumentResult::unchanged : DocumentResult::redraw;
    try {
        execute();
    } catch (const Error& error) {
        console_.set_error(error.what());
    }
    return DocumentResult::redraw;
}

void SqlDocument::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    const int editor_height = result_grid_ && bounds.height >= 13 ? 7 : bounds.height;
    result_pane_visible_ = result_grid_.has_value() && editor_height < bounds.height;
    if (!result_pane_visible_)
        result_focused_ = false;
    console_.set_focused(!result_focused_);
    console_.render(buffer, {bounds.x, bounds.y, bounds.width, editor_height});
    if (result_grid_ && result_pane_visible_) {
        result_grid_->set_focused(result_focused_);
        result_grid_->render(buffer, {bounds.x, bounds.y + editor_height, bounds.width, bounds.height - editor_height});
    }
}

} // namespace vulpes::ui
