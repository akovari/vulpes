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

SqlDocument::SqlDocument(db::Database& database, const core::Localizer& messages, const Theme& theme)
    : database_{&database}, messages_{&messages}, theme_{&theme},
      console_{messages.translate("sql.title"), messages.translate("sql.instructions"), theme} {
}

void SqlDocument::execute() {
    if (trim_ascii(console_.script()).empty())
        throw Error{ErrorCategory::validation, messages_->translate("sql.empty_error")};
    result_grid_.reset();
    auto result = database_->run_sql(console_.script());
    const auto rows = result.rows.size();
    const auto changes = result.changes;
    const auto truncated = result.truncated ? messages_->translate("sql.truncated") : std::string{};
    result_rows_ = GridRows::from_sql_result(std::move(result));
    if (!result_rows_->fields.empty())
        result_grid_.emplace(*result_rows_, messages_->translate("sql.results"),
                             messages_->translate("sql.result_footer"), *theme_);
    console_.set_status(messages_->translate(
        "sql.status",
        {{"rows", std::to_string(rows)}, {"changes", std::to_string(changes)}, {"truncated", truncated}}));
}

auto SqlDocument::handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult {
    if (result_grid_) {
        if (action == core::ActionId::dataset_previous && result_grid_->move_previous_row())
            return DocumentResult::redraw;
        if (action == core::ActionId::dataset_next && result_grid_->move_next_row())
            return DocumentResult::redraw;
        if (action == core::ActionId::grid_previous_column && result_grid_->move_left())
            return DocumentResult::redraw;
        if (action == core::ActionId::grid_next_column && result_grid_->move_right())
            return DocumentResult::redraw;
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
    console_.render(buffer, {bounds.x, bounds.y, bounds.width, editor_height});
    if (result_grid_ && editor_height < bounds.height)
        result_grid_->render(buffer, {bounds.x, bounds.y + editor_height, bounds.width, bounds.height - editor_height});
}

} // namespace vulpes::ui
