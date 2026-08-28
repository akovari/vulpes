#pragma once

#include "vulpes/core/clipboard.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/focus_ring.hpp"
#include "vulpes/ui/geometry.hpp"
#include "vulpes/ui/line_editor.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vulpes::ui {

enum class FormFieldKind { text, number, checkbox, lookup, read_only };
enum class FormMode { insert, edit };
enum class FormResult { unchanged, redraw, saved, cancelled };

struct FormField {
    std::string name;
    std::string label;
    FormFieldKind kind{FormFieldKind::text};
    bool read_only{false};
    LineEditor editor;
    std::vector<model::LookupOption> lookup_options;
    std::optional<std::size_t> selected_lookup_option;
    std::string error;
};

// A schema-generated, terminal-independent record editor. It owns temporary
// text input but delegates validation and persistence to Dataset.
class RecordForm {
  public:
    RecordForm(model::Dataset& dataset, std::string title, FormMode mode, std::string instructions,
               const Theme& theme = ui::theme(ThemeName::midnight), core::Clipboard* clipboard = nullptr);

    [[nodiscard]] auto fields() const noexcept -> const std::vector<FormField>& { return fields_; }
    [[nodiscard]] auto selected_field_index() const noexcept -> std::size_t { return selected_field_; }
    [[nodiscard]] auto error_field_index() const noexcept -> std::optional<std::size_t> { return error_field_; }
    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> FormResult;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

  private:
    [[nodiscard]] static auto field_kind(const db::FieldSchema& field, FormMode mode, bool is_foreign_key)
        -> FormFieldKind;
    [[nodiscard]] static auto format_value(const db::Value& value, FormFieldKind kind) -> std::string;
    [[nodiscard]] static auto parse_value(const FormField& field) -> db::Value;
    void move_selection(int direction);
    void move_lookup_selection(FormField& field, int direction);
    void record_error(const Error& error);
    void clear_error(std::size_t field);
    void save();

    model::Dataset* dataset_;
    std::string title_;
    std::string instructions_;
    std::vector<FormField> fields_;
    std::vector<bool> changed_;
    FocusRing field_focus_;
    std::size_t selected_field_{};
    std::optional<std::size_t> error_field_;
    std::string error_;
    const Theme* theme_;
    core::Clipboard* clipboard_;
};

} // namespace vulpes::ui
