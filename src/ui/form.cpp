#include "vulpes/ui/form.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/core/formatting.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <limits>
#include <numeric>
#include <type_traits>
#include <variant>

namespace vulpes::ui {
namespace {

auto lowercase_ascii(std::string text) -> std::string {
    for (auto& character : text) {
        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return text;
}

auto has_numeric_type(std::string_view type) -> bool {
    const auto upper = lowercase_ascii(std::string{type});
    return upper.find("int") != std::string::npos || upper.find("real") != std::string::npos ||
           upper.find("floa") != std::string::npos || upper.find("doub") != std::string::npos ||
           upper.find("num") != std::string::npos || upper.find("dec") != std::string::npos;
}

auto has_boolean_hint(const db::FieldSchema& field) -> bool {
    const auto name = lowercase_ascii(field.name);
    const auto type = lowercase_ascii(field.declared_type);
    return type.find("bool") != std::string::npos || name == "active" || name == "enabled" || name == "disabled" ||
           name.starts_with("is_") || name.starts_with("has_");
}

auto has_blob_type(std::string_view type) -> bool {
    return lowercase_ascii(std::string{type}).find("blob") != std::string::npos;
}

void write_padded(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text,
                  terminal::Style style = {}) {
    const auto clipped = terminal::truncate_utf8(text, width);
    const auto end = buffer.write_utf8(x, y, clipped, style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

RecordForm::RecordForm(model::Dataset& dataset, std::string title, FormMode mode, std::string instructions,
                       const Theme& theme, core::Clipboard* clipboard, const appmeta::TableMetadata* metadata)
    : dataset_{&dataset}, title_{std::move(title)}, instructions_{std::move(instructions)}, theme_{&theme},
      clipboard_{clipboard} {
    if (mode == FormMode::insert)
        dataset_->begin_insert();
    else
        dataset_->begin_edit();

    std::vector<std::size_t> field_order(dataset_->schema().fields.size());
    std::iota(field_order.begin(), field_order.end(), std::size_t{0});
    std::ranges::stable_sort(field_order, [&](std::size_t left, std::size_t right) {
        const auto* left_metadata =
            metadata == nullptr ? nullptr : metadata->field(dataset_->schema().fields[left].name);
        const auto* right_metadata =
            metadata == nullptr ? nullptr : metadata->field(dataset_->schema().fields[right].name);
        const auto left_order = left_metadata == nullptr ? std::nullopt : left_metadata->order;
        const auto right_order = right_metadata == nullptr ? std::nullopt : right_metadata->order;
        if (!left_order && !right_order)
            return false;
        return left_order.value_or(std::numeric_limits<std::size_t>::max()) <
               right_order.value_or(std::numeric_limits<std::size_t>::max());
    });

    std::vector<bool> focusable;
    for (const auto field_index : field_order) {
        const auto& schema_field = dataset_->schema().fields[field_index];
        if (schema_field.hidden)
            continue;
        const auto* field_metadata = metadata == nullptr ? nullptr : metadata->field(schema_field.name);
        if (field_metadata != nullptr && field_metadata->visible == false)
            continue;
        const bool is_foreign_key = std::ranges::any_of(dataset_->schema().foreign_keys, [&](const auto& foreign_key) {
            return foreign_key.field == schema_field.name;
        });
        auto kind = field_kind(schema_field, mode, is_foreign_key, field_metadata);
        if (field_metadata != nullptr && field_metadata->read_only == true)
            kind = FormFieldKind::read_only;
        const auto value = dataset_->draft_value(schema_field.name);
        FormField form_field{.name = schema_field.name,
                             .label = field_metadata != nullptr && field_metadata->label ? *field_metadata->label
                                                                                         : schema_field.name,
                             .kind = kind,
                             .read_only = kind == FormFieldKind::read_only,
                             .editor = LineEditor{value ? format_value(*value, kind) : std::string{}}};
        if (kind == FormFieldKind::lookup) {
            if (field_metadata != nullptr && field_metadata->lookup) {
                form_field.lookup_query.display_field = field_metadata->lookup->display_field;
                form_field.lookup_query.search_fields = field_metadata->lookup->search_fields;
                form_field.lookup_query.limit = field_metadata->lookup->result_limit;
                form_field.allow_drill_down = field_metadata->lookup->allow_drill_down;
            }
            form_field.lookup_options = dataset_->lookup_options(schema_field.name, form_field.lookup_query);
            if (value) {
                const auto selected = std::ranges::find(form_field.lookup_options, *value, &model::LookupOption::value);
                if (selected != form_field.lookup_options.end()) {
                    form_field.selected_lookup_option =
                        static_cast<std::size_t>(std::distance(form_field.lookup_options.begin(), selected));
                    form_field.editor.set_text(selected->label);
                }
            }
        }
        focusable.push_back(!form_field.read_only);
        fields_.push_back(std::move(form_field));
        changed_.push_back(false);
    }
    field_focus_.reset(std::move(focusable));
    if (const auto focused = field_focus_.current())
        selected_field_ = *focused;
}

auto RecordForm::handle(const terminal::InputEvent& event) -> FormResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (std::holds_alternative<terminal::ResizeEvent>(event))
        return FormResult::redraw;
    if (key != nullptr && key->key == terminal::Key::escape) {
        dataset_->cancel();
        return FormResult::cancelled;
    }
    if (key != nullptr && key->key == terminal::Key::f8) {
        try {
            save();
            return FormResult::saved;
        } catch (const Error& error) {
            record_error(error);
            return FormResult::redraw;
        }
    }
    if (key != nullptr && (key->key == terminal::Key::up || (key->key == terminal::Key::tab && key->shift))) {
        move_selection(-1);
        return FormResult::redraw;
    }
    if (key != nullptr && key->key == terminal::Key::enter && !fields_.empty() &&
        fields_[selected_field_].kind == FormFieldKind::lookup) {
        return FormResult::lookup_requested;
    }
    if (key != nullptr && (key->key == terminal::Key::down || key->key == terminal::Key::tab)) {
        move_selection(1);
        return FormResult::redraw;
    }
    if (fields_.empty() || fields_[selected_field_].read_only)
        return FormResult::unchanged;

    auto& field = fields_[selected_field_];
    if (key != nullptr && field.kind == FormFieldKind::lookup &&
        (key->key == terminal::Key::left || key->key == terminal::Key::right)) {
        move_lookup_selection(field, key->key == terminal::Key::left ? -1 : 1);
        changed_[selected_field_] = true;
        clear_error(selected_field_);
        return FormResult::redraw;
    }
    if (key != nullptr && field.kind == FormFieldKind::checkbox && key->key == terminal::Key::character &&
        key->character == U' ') {
        field.editor.set_text(field.editor.text() == "1" ? "0" : "1");
        changed_[selected_field_] = true;
        clear_error(selected_field_);
        return FormResult::redraw;
    }
    if (field.kind == FormFieldKind::text || field.kind == FormFieldKind::number || field.kind == FormFieldKind::date ||
        field.kind == FormFieldKind::time || field.kind == FormFieldKind::date_time) {
        const auto edit_result = field.editor.handle(event, clipboard_);
        if (edit_result == LineEditResult::changed) {
            changed_[selected_field_] = true;
            clear_error(selected_field_);
        }
        if (edit_result != LineEditResult::unchanged)
            return FormResult::redraw;
    }
    return FormResult::unchanged;
}

auto RecordForm::is_dirty() const noexcept -> bool {
    return std::ranges::any_of(changed_, [](bool changed) { return changed; });
}

auto RecordForm::lookup_request() const -> std::optional<LookupOpenRequest> {
    if (fields_.empty() || fields_[selected_field_].kind != FormFieldKind::lookup)
        return std::nullopt;
    const auto& field = fields_[selected_field_];
    auto query = field.lookup_query;
    query.search.clear();
    return LookupOpenRequest{
        .field = field.name, .query = std::move(query), .allow_drill_down = field.allow_drill_down};
}

void RecordForm::select_lookup(std::string_view field_name, model::LookupOption option) {
    const auto field = std::ranges::find(fields_, field_name, &FormField::name);
    if (field == fields_.end() || field->kind != FormFieldKind::lookup)
        throw Error{ErrorCategory::validation, "form field is not a relationship lookup: " + std::string{field_name}};
    auto selected = std::ranges::find(field->lookup_options, option.value, &model::LookupOption::value);
    if (selected == field->lookup_options.end()) {
        field->lookup_options.push_back(std::move(option));
        selected = std::prev(field->lookup_options.end());
    }
    field->selected_lookup_option = static_cast<std::size_t>(std::distance(field->lookup_options.begin(), selected));
    field->editor.set_text(selected->label);
    const auto index = static_cast<std::size_t>(std::distance(fields_.begin(), field));
    changed_[index] = true;
    clear_error(index);
}

void RecordForm::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 20, 6))
        return;
    const int interior = bounds.width - 2;
    WindowFrame::render(buffer, bounds, title_, window_frame_appearance(*theme_, true));

    const int label_width = std::min(18, interior / 2);
    const int value_width = interior - label_width;
    const int maximum_fields = bounds.height - 4;
    const auto first_visible_field = selected_field_ < static_cast<std::size_t>(maximum_fields)
                                         ? std::size_t{0}
                                         : selected_field_ - static_cast<std::size_t>(maximum_fields) + 1;
    for (int index = 0; index < maximum_fields; ++index) {
        const int y = bounds.y + 1 + index;
        const auto field_index = first_visible_field + static_cast<std::size_t>(index);
        if (field_index < fields_.size()) {
            const auto& field = fields_[field_index];
            const bool selected = field_index == selected_field_;
            write_padded(buffer, bounds.x + 1, y, label_width, field.label + ':',
                         theme_->style(field.error.empty() ? ThemeRole::text : ThemeRole::error));
            std::string value{field.editor.text()};
            if (field.kind == FormFieldKind::checkbox)
                value = field.editor.text() == "1" ? "✓" : " ";
            const auto role = field.read_only ? ThemeRole::muted_text
                              : selected      ? ThemeRole::input_focus
                                              : ThemeRole::input;
            if (field.kind == FormFieldKind::text || field.kind == FormFieldKind::number ||
                field.kind == FormFieldKind::date || field.kind == FormFieldKind::time ||
                field.kind == FormFieldKind::date_time) {
                field.editor.render(buffer, {bounds.x + 1 + label_width, y, value_width, 1}, theme_->style(role),
                                    selected);
            } else {
                write_padded(buffer, bounds.x + 1 + label_width, y, value_width, " " + value, theme_->style(role));
            }
        }
    }
    if (first_visible_field > 0)
        buffer.put(bounds.x + bounds.width - 1, bounds.y + 1, U'▲', theme_->style(ThemeRole::border));
    if (first_visible_field + static_cast<std::size_t>(maximum_fields) < fields_.size())
        buffer.put(bounds.x + bounds.width - 1, bounds.y + bounds.height - 3, U'▼', theme_->style(ThemeRole::border));
    const int footer_y = bounds.y + bounds.height - 2;
    write_padded(buffer, bounds.x + 1, footer_y, interior, error_.empty() ? instructions_ : error_,
                 theme_->style(error_.empty() ? ThemeRole::muted_text : ThemeRole::error));
}

auto RecordForm::field_kind(const db::FieldSchema& field, FormMode mode, bool is_foreign_key,
                            const appmeta::FieldMetadata* metadata) -> FormFieldKind {
    // Binary data needs a dedicated editor. Rendering it as text is useful for
    // inspection, but converting an edited placeholder back to SQLite TEXT is
    // never a safe default.
    if (field.generated || has_blob_type(field.declared_type) || (mode == FormMode::edit && field.primary_key))
        return FormFieldKind::read_only;
    if (is_foreign_key)
        return FormFieldKind::lookup;
    if (metadata != nullptr) {
        switch (metadata->format) {
        case appmeta::FieldFormat::text:
            return FormFieldKind::text;
        case appmeta::FieldFormat::number:
        case appmeta::FieldFormat::currency:
            return FormFieldKind::number;
        case appmeta::FieldFormat::boolean:
            return FormFieldKind::checkbox;
        case appmeta::FieldFormat::date:
            return FormFieldKind::date;
        case appmeta::FieldFormat::time:
            return FormFieldKind::time;
        case appmeta::FieldFormat::date_time:
            return FormFieldKind::date_time;
        case appmeta::FieldFormat::automatic:
            break;
        }
    }
    if (has_boolean_hint(field))
        return FormFieldKind::checkbox;
    if (has_numeric_type(field.declared_type))
        return FormFieldKind::number;
    return FormFieldKind::text;
}

auto RecordForm::format_value(const db::Value& value, FormFieldKind kind) -> std::string {
    return std::visit(
        [kind](const auto& item) -> std::string {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                return {};
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return kind == FormFieldKind::checkbox ? (item == 0 ? "0" : "1") : std::to_string(item);
            else if constexpr (std::is_same_v<T, double>)
                return std::to_string(item);
            else if constexpr (std::is_same_v<T, std::string>)
                return item;
            else
                return "<blob " + std::to_string(item.size()) + " bytes>";
        },
        value.storage());
}

auto RecordForm::parse_value(const FormField& field) -> db::Value {
    if (field.kind == FormFieldKind::lookup) {
        if (!field.selected_lookup_option)
            return db::Value{nullptr};
        return field.lookup_options.at(*field.selected_lookup_option).value;
    }
    if (field.kind == FormFieldKind::checkbox)
        return db::Value{field.editor.text() == "1"};
    if (field.kind == FormFieldKind::date || field.kind == FormFieldKind::time ||
        field.kind == FormFieldKind::date_time) {
        try {
            if (field.editor.text().empty())
                return db::Value{nullptr};
            if (field.kind == FormFieldKind::date)
                return db::Value{core::normalize_iso_date(field.editor.text())};
            if (field.kind == FormFieldKind::time)
                return db::Value{core::normalize_iso_time(field.editor.text())};
            return db::Value{core::normalize_rfc3339(field.editor.text())};
        } catch (const Error& error) {
            throw Error{ErrorCategory::validation,
                        "invalid date/time for field " + field.name + ": " + std::string{error.what()}};
        }
    }
    if (field.kind != FormFieldKind::number)
        return db::Value{std::string{field.editor.text()}};
    const auto text = field.editor.text();
    if (text.empty())
        return db::Value{nullptr};

    if (text.find_first_of(".eE") == std::string_view::npos) {
        std::int64_t value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error == std::errc{} && end == text.data() + text.size())
            return db::Value{value};
    } else {
        double value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error == std::errc{} && end == text.data() + text.size())
            return db::Value{value};
    }
    throw Error{ErrorCategory::validation, "invalid number for field: " + field.name};
}

void RecordForm::move_selection(int direction) {
    if (field_focus_.move(direction))
        selected_field_ = *field_focus_.current();
}

void RecordForm::move_lookup_selection(FormField& field, int direction) {
    if (field.lookup_options.empty())
        return;
    const auto count = static_cast<int>(field.lookup_options.size());
    const auto next = field.selected_lookup_option
                          ? (static_cast<int>(*field.selected_lookup_option) + direction + count) % count
                          : (direction < 0 ? count - 1 : 0);
    field.selected_lookup_option = static_cast<std::size_t>(next);
    field.editor.set_text(field.lookup_options.at(*field.selected_lookup_option).label);
}

void RecordForm::record_error(const Error& error) {
    error_ = error.what();
    error_field_.reset();
    for (auto& field : fields_)
        field.error.clear();

    const auto message = std::string_view{error.what()};
    for (std::size_t index = 0; index < fields_.size(); ++index) {
        const auto& name = fields_[index].name;
        if (message.find(name) == std::string_view::npos)
            continue;
        error_field_ = index;
        if (field_focus_.select(index))
            selected_field_ = index;
        fields_[index].error = error_;
        return;
    }

    const auto changed = std::ranges::find(changed_, true);
    if (changed != changed_.end() && std::ranges::count(changed_, true) == 1) {
        const auto index = static_cast<std::size_t>(std::distance(changed_.begin(), changed));
        error_field_ = index;
        if (field_focus_.select(index))
            selected_field_ = index;
        fields_[index].error = error_;
    }
}

void RecordForm::clear_error(std::size_t field) {
    fields_[field].error.clear();
    if (error_field_ == field) {
        error_field_.reset();
        error_.clear();
    }
}

void RecordForm::save() {
    std::vector<std::pair<std::string, db::Value>> changes;
    for (std::size_t index = 0; index < fields_.size(); ++index) {
        if (changed_[index] && !fields_[index].read_only)
            changes.emplace_back(fields_[index].name, parse_value(fields_[index]));
    }
    for (const auto& [name, value] : changes)
        dataset_->set(name, value);
    dataset_->save();
}

} // namespace vulpes::ui
