#include "vulpes/ui/browse_document.hpp"

#include "vulpes/core/error.hpp"

#include <cctype>
#include <charconv>

namespace vulpes::ui {
namespace {

auto trim_ascii(std::string_view text) -> std::string_view {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        text.remove_suffix(1);
    return text;
}

auto lowercase_ascii(std::string_view text) -> std::string {
    std::string result{text};
    for (auto& character : result)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return result;
}

auto is_numeric_field(const db::FieldSchema& field) -> bool {
    const auto type = lowercase_ascii(field.declared_type);
    return type.find("int") != std::string::npos || type.find("real") != std::string::npos ||
           type.find("floa") != std::string::npos || type.find("doub") != std::string::npos ||
           type.find("num") != std::string::npos || type.find("dec") != std::string::npos;
}

auto parse_filter(const db::FieldSchema& field, std::string_view source) -> model::Filter {
    auto text = trim_ascii(source);
    model::Filter result{.field = field.name};
    const auto consume = [&](std::string_view prefix, model::FilterOperator comparison) {
        if (!text.starts_with(prefix))
            return false;
        result.comparison = comparison;
        text = trim_ascii(text.substr(prefix.size()));
        return true;
    };
    static_cast<void>(consume(">=", model::FilterOperator::greater_equal) ||
                      consume("<=", model::FilterOperator::less_equal) ||
                      consume("!=", model::FilterOperator::not_equal) ||
                      consume("<>", model::FilterOperator::not_equal) || consume(">", model::FilterOperator::greater) ||
                      consume("<", model::FilterOperator::less) || consume("=", model::FilterOperator::equal));
    if (text.empty())
        throw Error{ErrorCategory::validation, "a filter value is required"};
    if (lowercase_ascii(text) == "null") {
        result.value = nullptr;
        return result;
    }
    if (!is_numeric_field(field)) {
        result.value = std::string{text};
        return result;
    }
    if (text.find_first_of(".eE") == std::string_view::npos) {
        std::int64_t value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error == std::errc{} && end == text.data() + text.size()) {
            result.value = value;
            return result;
        }
    } else {
        double value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error == std::errc{} && end == text.data() + text.size()) {
            result.value = value;
            return result;
        }
    }
    throw Error{ErrorCategory::validation, "invalid number for filter: " + field.name};
}

} // namespace

BrowseDocument::BrowseDocument(db::Database& database, db::TableSchema table, const core::Localizer& messages,
                               const Theme& theme)
    : messages_{&messages}, theme_{&theme}, dataset_{database, std::move(table)}, controller_{dataset_},
      grid_{dataset_, dataset_.schema().name,
            messages.translate(database.is_read_only() ? "browse.read_only_footer" : "browse.footer"), theme} {
}

void BrowseDocument::begin_form(FormMode mode) {
    const auto title_key = mode == FormMode::edit ? "form.edit_title" : "form.new_title";
    form_.emplace(dataset_, messages_->translate(title_key, {{"table", dataset_.schema().name}}), mode,
                  messages_->translate("form.instructions"), *theme_);
}

void BrowseDocument::begin_prompt(PromptPurpose purpose) {
    prompt_purpose_ = purpose;
    if (purpose == PromptPurpose::search)
        prompt_.emplace(messages_->translate("browse.search_prompt"), messages_->translate("prompt.instructions"),
                        std::string{}, *theme_);
    else {
        const auto* field = grid_.selected_field();
        if (field != nullptr)
            prompt_.emplace(messages_->translate("browse.filter_prompt", {{"field", field->name}}),
                            messages_->translate("prompt.instructions"), std::string{}, *theme_);
    }
}

void BrowseDocument::begin_delete_confirmation() {
    confirmation_.emplace(messages_->translate("browse.delete_title"),
                          messages_->translate("browse.delete_message", {{"table", dataset_.schema().name}}),
                          messages_->translate("dialog.delete"), messages_->translate("dialog.cancel"),
                          messages_->translate("dialog.select"), *theme_);
}

void BrowseDocument::apply_prompt() {
    if (!prompt_)
        return;
    const auto text = prompt_->value();
    if (prompt_purpose_ == PromptPurpose::search) {
        if (text.empty())
            dataset_.clear_search();
        else
            dataset_.search(text);
    } else if (prompt_purpose_ == PromptPurpose::filter) {
        const auto* field = grid_.selected_field();
        if (field == nullptr)
            return;
        if (text.empty())
            dataset_.clear_filters();
        else
            dataset_.where(parse_filter(*field, text));
    }
    prompt_.reset();
    prompt_purpose_ = PromptPurpose::none;
}

auto BrowseDocument::handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult {
    if (form_) {
        const auto result = form_->handle(event);
        if (result == FormResult::saved || result == FormResult::cancelled)
            form_.reset();
        return result == FormResult::unchanged ? DocumentResult::unchanged : DocumentResult::redraw;
    }
    if (prompt_) {
        const auto result = prompt_->handle(event);
        if (result == PromptResult::cancelled) {
            prompt_.reset();
            prompt_purpose_ = PromptPurpose::none;
            return DocumentResult::redraw;
        }
        if (result != PromptResult::submitted)
            return result == PromptResult::unchanged ? DocumentResult::unchanged : DocumentResult::redraw;
        try {
            apply_prompt();
            return DocumentResult::redraw;
        } catch (const Error& error) {
            prompt_->set_error(error.what());
            return DocumentResult::redraw;
        }
    }
    if (confirmation_) {
        const auto result = confirmation_->handle(event);
        if (result == ConfirmationResult::confirmed) {
            dataset_.erase();
            confirmation_.reset();
            return DocumentResult::redraw;
        }
        if (result == ConfirmationResult::cancelled) {
            confirmation_.reset();
            return DocumentResult::redraw;
        }
        return result == ConfirmationResult::unchanged ? DocumentResult::unchanged : DocumentResult::redraw;
    }

    switch (action) {
    case core::ActionId::record_edit:
        if (dataset_.is_editable() && dataset_.current())
            begin_form(FormMode::edit);
        return DocumentResult::redraw;
    case core::ActionId::record_new:
        if (dataset_.is_editable())
            begin_form(FormMode::insert);
        return DocumentResult::redraw;
    case core::ActionId::record_delete:
        if (dataset_.is_editable() && dataset_.current())
            begin_delete_confirmation();
        return DocumentResult::redraw;
    case core::ActionId::dataset_search:
        begin_prompt(PromptPurpose::search);
        return DocumentResult::redraw;
    case core::ActionId::dataset_filter:
        begin_prompt(PromptPurpose::filter);
        return DocumentResult::redraw;
    case core::ActionId::dataset_refresh:
        dataset_.refresh();
        return DocumentResult::redraw;
    case core::ActionId::dataset_sort: {
        const auto* field = grid_.selected_field();
        if (field == nullptr)
            return DocumentResult::unchanged;
        const auto direction = sort_ && sort_->first == field->name && sort_->second == model::SortDirection::ascending
                                   ? model::SortDirection::descending
                                   : model::SortDirection::ascending;
        dataset_.order_by(field->name, direction);
        sort_ = std::pair{field->name, direction};
        return DocumentResult::redraw;
    }
    case core::ActionId::grid_previous_column:
        return grid_.move_left() ? DocumentResult::redraw : DocumentResult::unchanged;
    case core::ActionId::grid_next_column:
        return grid_.move_right() ? DocumentResult::redraw : DocumentResult::unchanged;
    default:
        break;
    }
    const auto result = controller_.handle(action);
    if (result == core::BrowseResult::close)
        return DocumentResult::close;
    return result == core::BrowseResult::redraw ? DocumentResult::redraw : DocumentResult::unchanged;
}

void BrowseDocument::render(terminal::ScreenBuffer& buffer, Rect bounds) {
    grid_.render(buffer, bounds);
    if (form_) {
        const int dialog_width = std::min(72, bounds.width);
        const int requested_height = static_cast<int>(form_->fields().size()) + 4;
        const int dialog_height = std::clamp(requested_height, 6, bounds.height);
        form_->render(buffer, {bounds.x + (bounds.width - dialog_width) / 2,
                               bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
        return;
    }
    if (prompt_) {
        constexpr int prompt_height = 5;
        const int prompt_width = std::min(64, bounds.width);
        prompt_->render(buffer, {bounds.x + (bounds.width - prompt_width) / 2,
                                 bounds.y + (bounds.height - prompt_height) / 2, prompt_width, prompt_height});
        return;
    }
    if (confirmation_) {
        constexpr int dialog_height = 6;
        const int dialog_width = std::min(bounds.width, 60);
        confirmation_->render(buffer, {bounds.x + (bounds.width - dialog_width) / 2,
                                       bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
    }
}

} // namespace vulpes::ui
