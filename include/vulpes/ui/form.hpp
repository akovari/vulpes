#pragma once

#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"

#include <string>
#include <vector>

namespace vulpes::ui {

enum class FormFieldKind { text, number, checkbox, read_only };
enum class FormMode { insert, edit };
enum class FormResult { unchanged, redraw, saved, cancelled };

struct FormField {
    std::string name;
    std::string label;
    FormFieldKind kind{FormFieldKind::text};
    bool read_only{false};
    std::string text;
};

// A schema-generated, terminal-independent record editor. It owns temporary
// text input but delegates validation and persistence to Dataset.
class RecordForm {
  public:
    RecordForm(model::Dataset& dataset, std::string title, FormMode mode);

    [[nodiscard]] auto fields() const noexcept -> const std::vector<FormField>& { return fields_; }
    [[nodiscard]] auto selected_field_index() const noexcept -> std::size_t { return selected_field_; }
    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> FormResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    [[nodiscard]] static auto field_kind(const db::FieldSchema& field, FormMode mode) -> FormFieldKind;
    [[nodiscard]] static auto format_value(const db::Value& value, FormFieldKind kind) -> std::string;
    [[nodiscard]] static auto parse_value(const FormField& field) -> db::Value;
    void move_selection(int direction);
    void save();

    model::Dataset* dataset_;
    std::string title_;
    std::vector<FormField> fields_;
    std::vector<bool> changed_;
    std::size_t selected_field_{};
    std::string error_;
};

} // namespace vulpes::ui
