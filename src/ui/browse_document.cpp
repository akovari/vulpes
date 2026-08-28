#include "vulpes/ui/browse_document.hpp"

#include "vulpes/core/error.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <type_traits>

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

auto table_metadata(const appmeta::ApplicationMetadata* metadata, const std::optional<appmeta::TableMetadata>& override,
                    std::string_view table) -> std::optional<appmeta::TableMetadata> {
    if (override)
        return override;
    if (metadata != nullptr) {
        if (const auto* result = metadata->table(table); result != nullptr)
            return *result;
    }
    return std::nullopt;
}

auto table_label(const appmeta::ApplicationMetadata* metadata, const appmeta::TableMetadata* override,
                 std::string_view table) -> std::string {
    if (override != nullptr && override->label)
        return *override->label;
    if (metadata != nullptr) {
        if (const auto* result = metadata->table(table); result != nullptr && result->label)
            return *result->label;
    }
    return std::string{table};
}

} // namespace

BrowseDocument::BrowseDocument(db::Database& database, db::TableSchema table, const core::Localizer& messages,
                               const Theme& theme, core::Clipboard* clipboard,
                               const appmeta::ApplicationMetadata* metadata,
                               std::optional<appmeta::TableMetadata> table_override)
    : messages_{&messages}, metadata_{metadata}, table_override_{std::move(table_override)}, theme_{&theme},
      clipboard_{clipboard}, dataset_{database, std::move(table)}, controller_{dataset_},
      grid_{dataset_,
            table_label(metadata, table_override_ ? &*table_override_ : nullptr, dataset_.schema().name),
            messages.translate(database.is_read_only() ? "browse.read_only_footer" : "browse.footer"),
            theme,
            {.empty = messages.translate("grid.empty"),
             .row = messages.translate("grid.row"),
             .rows = messages.translate("grid.rows"),
             .column = messages.translate("grid.column")},
            core::LocaleFormatter{std::string{messages.locale()}},
            table_metadata(metadata, table_override_, dataset_.schema().name)} {
}

auto BrowseDocument::current_table_metadata() const noexcept -> const appmeta::TableMetadata* {
    if (table_override_)
        return &*table_override_;
    return metadata_ == nullptr ? nullptr : metadata_->table(dataset_.schema().name);
}

auto BrowseDocument::is_dirty() const noexcept -> bool {
    return std::ranges::any_of(windows_.layers(), [](const auto& layer) {
        const auto* form = std::get_if<RecordForm>(&layer.content);
        return form != nullptr && form->is_dirty();
    });
}

void BrowseDocument::begin_form(FormMode mode) {
    const auto title_key = mode == FormMode::edit ? "form.edit_title" : "form.new_title";
    auto title = messages_->translate(
        title_key, {{"table", table_label(metadata_, current_table_metadata(), dataset_.schema().name)}});
    windows_.push({.id = "record-form", .title = title, .kind = WindowLayerKind::form},
                  BrowseWindow{std::in_place_type<RecordForm>, dataset_, std::move(title), mode,
                               messages_->translate("form.instructions"), *theme_, clipboard_,
                               current_table_metadata()});
}

void BrowseDocument::begin_prompt(PromptPurpose purpose) {
    if (purpose == PromptPurpose::search) {
        auto title = messages_->translate("browse.search_prompt");
        windows_.push({.id = "search", .title = title, .kind = WindowLayerKind::prompt},
                      BrowseWindow{std::in_place_type<PromptWindow>, purpose,
                                   TextPrompt{std::move(title), messages_->translate("prompt.instructions"),
                                              std::string{}, *theme_, clipboard_}});
    } else {
        const auto* field = grid_.selected_field();
        if (field != nullptr) {
            auto title = messages_->translate("browse.filter_prompt", {{"field", field->name}});
            windows_.push({.id = "filter", .title = title, .kind = WindowLayerKind::prompt},
                          BrowseWindow{std::in_place_type<PromptWindow>, purpose,
                                       TextPrompt{std::move(title), messages_->translate("prompt.instructions"),
                                                  std::string{}, *theme_, clipboard_}});
        }
    }
}

void BrowseDocument::begin_delete_confirmation() {
    auto title = messages_->translate("browse.delete_title");
    windows_.push({.id = "delete-confirmation", .title = title, .kind = WindowLayerKind::dialog},
                  BrowseWindow{std::in_place_type<ConfirmationDialog>, std::move(title),
                               messages_->translate("browse.delete_message", {{"table", dataset_.schema().name}}),
                               messages_->translate("dialog.delete"), messages_->translate("dialog.cancel"),
                               messages_->translate("dialog.select"), *theme_});
}

void BrowseDocument::apply_prompt(PromptWindow& window) {
    const auto text = window.prompt.value();
    if (window.purpose == PromptPurpose::search) {
        if (text.empty())
            dataset_.clear_search();
        else
            dataset_.search(text);
    } else if (window.purpose == PromptPurpose::filter) {
        const auto* field = grid_.selected_field();
        if (field == nullptr)
            return;
        if (text.empty())
            dataset_.clear_filters();
        else
            dataset_.where(parse_filter(*field, text));
    }
}

auto BrowseDocument::handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult {
    if (!windows_.empty()) {
        bool pop_window{false};
        std::optional<LookupOpenRequest> open_lookup;
        std::optional<model::LookupOption> selected_lookup;
        std::string selected_lookup_field;
        std::optional<model::RelatedRecord> open_related;
        const auto outcome = std::visit(
            [&](auto& window) -> DocumentResult {
                using Window = std::remove_cvref_t<decltype(window)>;
                if constexpr (std::is_same_v<Window, RecordForm>) {
                    const auto result = window.handle(event);
                    windows_.top().descriptor.dirty = window.is_dirty();
                    if (result == FormResult::saved || result == FormResult::cancelled)
                        pop_window = true;
                    else if (result == FormResult::lookup_requested)
                        open_lookup = window.lookup_request();
                    return result == FormResult::unchanged ? DocumentResult::unchanged : DocumentResult::redraw;
                } else if constexpr (std::is_same_v<Window, PromptWindow>) {
                    const auto result = window.prompt.handle(event);
                    if (result == PromptResult::cancelled) {
                        pop_window = true;
                        return DocumentResult::redraw;
                    }
                    if (result != PromptResult::submitted)
                        return result == PromptResult::unchanged ? DocumentResult::unchanged : DocumentResult::redraw;
                    try {
                        apply_prompt(window);
                        pop_window = true;
                    } catch (const Error& error) {
                        window.prompt.set_error(error.what());
                    }
                    return DocumentResult::redraw;
                } else if constexpr (std::is_same_v<Window, ConfirmationDialog>) {
                    const auto result = window.handle(event);
                    if (result == ConfirmationResult::confirmed) {
                        dataset_.erase();
                        pop_window = true;
                        return DocumentResult::redraw;
                    }
                    if (result == ConfirmationResult::cancelled) {
                        pop_window = true;
                        return DocumentResult::redraw;
                    }
                    return result == ConfirmationResult::unchanged ? DocumentResult::unchanged : DocumentResult::redraw;
                } else if constexpr (std::is_same_v<Window, RelationshipLookup>) {
                    const auto result = window.handle(event);
                    if (result == RelationshipLookupResult::selected) {
                        selected_lookup = *window.selected_option();
                        selected_lookup_field = std::string{window.field()};
                        pop_window = true;
                    } else if (result == RelationshipLookupResult::cancelled) {
                        pop_window = true;
                    } else if (result == RelationshipLookupResult::drill_down) {
                        open_related = dataset_.related_record(window.field(), window.selected_option()->value);
                    }
                    return result == RelationshipLookupResult::unchanged ? DocumentResult::unchanged
                                                                         : DocumentResult::redraw;
                } else {
                    const auto result = window.handle(event);
                    if (result == RelatedRecordResult::cancelled)
                        pop_window = true;
                    return result == RelatedRecordResult::unchanged ? DocumentResult::unchanged
                                                                    : DocumentResult::redraw;
                }
            },
            windows_.top().content);
        if (pop_window)
            static_cast<void>(windows_.pop());
        if (selected_lookup && !windows_.empty()) {
            if (auto* form = std::get_if<RecordForm>(&windows_.top().content))
                form->select_lookup(selected_lookup_field, std::move(*selected_lookup));
        }
        if (open_lookup) {
            auto label = open_lookup->field;
            if (const auto* table = current_table_metadata(); table != nullptr) {
                if (const auto* field = table->field(open_lookup->field); field != nullptr && field->label)
                    label = *field->label;
            }
            auto title = messages_->translate("lookup.title", {{"field", label}});
            windows_.push({.id = "lookup:" + open_lookup->field, .title = title, .kind = WindowLayerKind::lookup},
                          BrowseWindow{std::in_place_type<RelationshipLookup>, dataset_, open_lookup->field,
                                       std::move(open_lookup->query), open_lookup->allow_drill_down, std::move(title),
                                       messages_->translate("lookup.search"),
                                       messages_->translate("lookup.instructions"), *theme_, clipboard_});
        }
        if (open_related) {
            const auto* table_metadata = metadata_ == nullptr ? nullptr : metadata_->table(open_related->schema.name);
            const auto label =
                table_metadata != nullptr && table_metadata->label ? *table_metadata->label : open_related->schema.name;
            auto title = messages_->translate("lookup.related_title", {{"table", label}});
            windows_.push(
                {.id = "related:" + open_related->schema.name, .title = title, .kind = WindowLayerKind::drill_down},
                BrowseWindow{std::in_place_type<RelatedRecordView>, std::move(*open_related), std::move(title),
                             messages_->translate("lookup.related_instructions"), *theme_, table_metadata});
        }
        return outcome;
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
    case core::ActionId::grid_narrow_column:
        return grid_.resize_selected_column(-1) ? DocumentResult::redraw : DocumentResult::unchanged;
    case core::ActionId::grid_widen_column:
        return grid_.resize_selected_column(1) ? DocumentResult::redraw : DocumentResult::unchanged;
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
    for (auto& layer : windows_.layers()) {
        std::visit(
            [&](auto& window) {
                using Window = std::remove_cvref_t<decltype(window)>;
                if constexpr (std::is_same_v<Window, RecordForm>) {
                    const int dialog_width = std::min(72, bounds.width);
                    const int requested_height = static_cast<int>(window.fields().size()) + 4;
                    const int dialog_height = std::clamp(requested_height, 6, bounds.height);
                    window.render(buffer,
                                  {bounds.x + (bounds.width - dialog_width) / 2,
                                   bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
                } else if constexpr (std::is_same_v<Window, PromptWindow>) {
                    constexpr int prompt_height = 5;
                    const int prompt_width = std::min(64, bounds.width);
                    window.prompt.render(buffer,
                                         {bounds.x + (bounds.width - prompt_width) / 2,
                                          bounds.y + (bounds.height - prompt_height) / 2, prompt_width, prompt_height});
                } else if constexpr (std::is_same_v<Window, ConfirmationDialog>) {
                    constexpr int dialog_height = 6;
                    const int dialog_width = std::min(bounds.width, 60);
                    window.render(buffer,
                                  {bounds.x + (bounds.width - dialog_width) / 2,
                                   bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
                } else if constexpr (std::is_same_v<Window, RelationshipLookup>) {
                    const int dialog_width = std::min(bounds.width, 70);
                    const int dialog_height = std::min(bounds.height, 14);
                    window.render(buffer,
                                  {bounds.x + (bounds.width - dialog_width) / 2,
                                   bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
                } else {
                    const int dialog_width = std::min(bounds.width, 72);
                    const int dialog_height = std::clamp(static_cast<int>(window.field_count()) + 3, 6, bounds.height);
                    window.render(buffer,
                                  {bounds.x + (bounds.width - dialog_width) / 2,
                                   bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
                }
            },
            layer.content);
    }
}

} // namespace vulpes::ui
