#include "vulpes/ui/report_document.hpp"

namespace vulpes::ui {

ReportDocument::ReportDocument(db::Database& database, appmeta::ReportDefinition report,
                               const core::Localizer& messages, const Theme& theme)
    : rows_{GridRows::from_sql_result(database.run_query(report.sql, report.row_limit))},
      grid_{rows_,
            std::move(report.label),
            messages.translate(rows_.truncated ? "report.footer_truncated" : "report.footer"),
            theme,
            {.empty = messages.translate("grid.empty"),
             .row = messages.translate("grid.row"),
             .rows = messages.translate("grid.rows"),
             .column = messages.translate("grid.column")},
            core::LocaleFormatter{std::string{messages.locale()}}} {
}

auto ReportDocument::handle(core::ActionId action, const terminal::InputEvent&) -> DocumentResult {
    if (action == core::ActionId::application_back)
        return DocumentResult::close;
    if (action == core::ActionId::dataset_previous && grid_.move_previous_row())
        return DocumentResult::redraw;
    if (action == core::ActionId::dataset_next && grid_.move_next_row())
        return DocumentResult::redraw;
    if (action == core::ActionId::grid_previous_column && grid_.move_left())
        return DocumentResult::redraw;
    if (action == core::ActionId::grid_next_column && grid_.move_right())
        return DocumentResult::redraw;
    if (action == core::ActionId::grid_narrow_column && grid_.resize_selected_column(-1))
        return DocumentResult::redraw;
    if (action == core::ActionId::grid_widen_column && grid_.resize_selected_column(1))
        return DocumentResult::redraw;
    if (action == core::ActionId::dataset_sort && grid_.sort_selected())
        return DocumentResult::redraw;
    return DocumentResult::unchanged;
}

void ReportDocument::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    grid_.render(buffer, bounds);
}

} // namespace vulpes::ui
