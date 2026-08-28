#pragma once

namespace vulpes::ui {

enum class TextEditMode { insert, overwrite };

struct TextEditorOptions {
    TextEditMode initial_mode{TextEditMode::insert};
    bool allow_mode_toggle{true};
};

} // namespace vulpes::ui
