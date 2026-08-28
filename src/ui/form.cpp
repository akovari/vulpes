#include "vulpes/ui/form.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <charconv>
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

void erase_last_code_point(std::string& text) {
    if (text.empty())
        return;
    std::size_t offset = text.size() - 1;
    while (offset > 0 && (static_cast<unsigned char>(text[offset]) & 0xC0U) == 0x80U)
        --offset;
    text.erase(offset);
}

} // namespace

RecordForm::RecordForm(model::Dataset& dataset, std::string title, FormMode mode, std::string instructions)
    : dataset_{&dataset}, title_{std::move(title)}, instructions_{std::move(instructions)} {
    if (mode == FormMode::insert)
        dataset_->begin_insert();
    else
        dataset_->begin_edit();

    std::vector<bool> focusable;
    for (const auto& schema_field : dataset_->schema().fields) {
        if (schema_field.hidden)
            continue;
        const bool is_foreign_key = std::ranges::any_of(dataset_->schema().foreign_keys, [&](const auto& foreign_key) {
            return foreign_key.field == schema_field.name;
        });
        const auto kind = field_kind(schema_field, mode, is_foreign_key);
        const auto value = dataset_->draft_value(schema_field.name);
        FormField form_field{.name = schema_field.name,
                             .label = schema_field.name,
                             .kind = kind,
                             .read_only = kind == FormFieldKind::read_only,
                             .text = value ? format_value(*value, kind) : std::string{}};
        if (kind == FormFieldKind::lookup) {
            form_field.lookup_options = dataset_->lookup_options(schema_field.name);
            if (value) {
                const auto selected = std::ranges::find(form_field.lookup_options, *value, &model::LookupOption::value);
                if (selected != form_field.lookup_options.end()) {
                    form_field.selected_lookup_option =
                        static_cast<std::size_t>(std::distance(form_field.lookup_options.begin(), selected));
                    form_field.text = selected->label;
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
    if (key == nullptr)
        return FormResult::redraw;
    if (key->key == terminal::Key::escape) {
        dataset_->cancel();
        return FormResult::cancelled;
    }
    if (key->key == terminal::Key::f8) {
        try {
            save();
            return FormResult::saved;
        } catch (const Error& error) {
            record_error(error);
            return FormResult::redraw;
        }
    }
    if (key->key == terminal::Key::up || (key->key == terminal::Key::tab && key->shift)) {
        move_selection(-1);
        return FormResult::redraw;
    }
    if (key->key == terminal::Key::down || key->key == terminal::Key::enter || key->key == terminal::Key::tab) {
        move_selection(1);
        return FormResult::redraw;
    }
    if (fields_.empty() || fields_[selected_field_].read_only)
        return FormResult::unchanged;

    auto& field = fields_[selected_field_];
    if (field.kind == FormFieldKind::lookup && (key->key == terminal::Key::left || key->key == terminal::Key::right)) {
        move_lookup_selection(field, key->key == terminal::Key::left ? -1 : 1);
        changed_[selected_field_] = true;
        clear_error(selected_field_);
        return FormResult::redraw;
    }
    if (key->key == terminal::Key::backspace) {
        erase_last_code_point(field.text);
        changed_[selected_field_] = true;
        clear_error(selected_field_);
        return FormResult::redraw;
    }
    if (field.kind == FormFieldKind::checkbox && key->key == terminal::Key::character && key->character == U' ') {
        field.text = field.text == "1" ? "0" : "1";
        changed_[selected_field_] = true;
        clear_error(selected_field_);
        return FormResult::redraw;
    }
    if (key->key == terminal::Key::character && !key->ctrl && !key->alt) {
        field.text += terminal::encode_utf8(key->character);
        changed_[selected_field_] = true;
        clear_error(selected_field_);
        return FormResult::redraw;
    }
    return FormResult::unchanged;
}

void RecordForm::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 20, 6))
        return;
    const int interior = bounds.width - 2;
    WindowFrame::render(buffer, bounds, title_);

    const int label_width = std::min(18, interior / 2);
    const int value_width = interior - label_width;
    const int maximum_fields = bounds.height - 4;
    for (int index = 0; index < maximum_fields; ++index) {
        const int y = bounds.y + 1 + index;
        buffer.put(bounds.x, y, U'|');
        if (static_cast<std::size_t>(index) < fields_.size()) {
            const auto& field = fields_[static_cast<std::size_t>(index)];
            const bool selected = static_cast<std::size_t>(index) == selected_field_;
            write_padded(buffer, bounds.x + 1, y, label_width, field.label + ':', {.bold = !field.error.empty()});
            std::string value = field.text;
            if (field.kind == FormFieldKind::checkbox)
                value = field.text == "1" ? "[x]" : "[ ]";
            const terminal::Style style{.underline = selected, .reverse = selected};
            write_padded(buffer, bounds.x + 1 + label_width, y, value_width, "[" + value + "]", style);
        }
        buffer.put(bounds.x + bounds.width - 1, y, U'|');
    }
    const int footer_y = bounds.y + bounds.height - 2;
    buffer.put(bounds.x, footer_y, U'|');
    write_padded(buffer, bounds.x + 2, footer_y, interior - 2, error_.empty() ? instructions_ : error_);
    buffer.put(bounds.x + bounds.width - 1, footer_y, U'|');
}

auto RecordForm::field_kind(const db::FieldSchema& field, FormMode mode, bool is_foreign_key) -> FormFieldKind {
    // Binary data needs a dedicated editor. Rendering it as text is useful for
    // inspection, but converting an edited placeholder back to SQLite TEXT is
    // never a safe default.
    if (field.generated || has_blob_type(field.declared_type) || (mode == FormMode::edit && field.primary_key))
        return FormFieldKind::read_only;
    if (is_foreign_key)
        return FormFieldKind::lookup;
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
        return db::Value{field.text == "1"};
    if (field.kind != FormFieldKind::number)
        return db::Value{field.text};
    if (field.text.empty())
        return db::Value{nullptr};

    if (field.text.find_first_of(".eE") == std::string::npos) {
        std::int64_t value{};
        const auto [end, error] = std::from_chars(field.text.data(), field.text.data() + field.text.size(), value);
        if (error == std::errc{} && end == field.text.data() + field.text.size())
            return db::Value{value};
    } else {
        double value{};
        const auto [end, error] = std::from_chars(field.text.data(), field.text.data() + field.text.size(), value);
        if (error == std::errc{} && end == field.text.data() + field.text.size())
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
    field.text = field.lookup_options.at(*field.selected_lookup_option).label;
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
