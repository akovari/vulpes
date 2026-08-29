#pragma once

#include "vulpes/appmeta/definition.hpp"
#include "vulpes/core/actions.hpp"
#include "vulpes/terminal/unicode.hpp"
#include "vulpes/ui/button.hpp"
#include "vulpes/ui/document_surface.hpp"
#include "vulpes/ui/theme.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vulpes::ui {

// The TUI presentation of an application screen is deliberately small: an
// ordered list of semantic command buttons. The SQLite-resident screen
// definition supplies no terminal coordinates, and the document returns only
// a command name for its host to dispatch through ApplicationRuntime.
class ScreenDocument final : public DocumentSurface {
  public:
    explicit ScreenDocument(appmeta::ScreenDefinition definition, const Theme& theme = ui::theme(ThemeName::midnight))
        : definition_{std::move(definition)}, theme_{&theme} {
        buttons_.reserve(definition_.items.size());
        for (const auto& item : definition_.items)
            buttons_.emplace_back(item.label);
    }

    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult override {
        if (action == core::ActionId::application_back)
            return DocumentResult::close;
        if (definition_.items.empty())
            return DocumentResult::unchanged;
        if (action == core::ActionId::dataset_first) {
            if (selected_item_ == 0)
                return DocumentResult::unchanged;
            selected_item_ = 0;
            return DocumentResult::redraw;
        }
        if (action == core::ActionId::dataset_last) {
            const auto final_item = definition_.items.size() - 1;
            if (selected_item_ == final_item)
                return DocumentResult::unchanged;
            selected_item_ = final_item;
            return DocumentResult::redraw;
        }
        if (action == core::ActionId::dataset_previous) {
            if (selected_item_ == 0)
                return DocumentResult::unchanged;
            --selected_item_;
            return DocumentResult::redraw;
        }
        if (action == core::ActionId::dataset_next) {
            if (selected_item_ + 1 >= definition_.items.size())
                return DocumentResult::unchanged;
            ++selected_item_;
            return DocumentResult::redraw;
        }
        const auto* key = std::get_if<terminal::KeyEvent>(&event);
        if (key != nullptr && key->key == terminal::Key::enter) {
            pending_command_ = definition_.items[selected_item_].command;
            return DocumentResult::command;
        }
        return DocumentResult::unchanged;
    }

    // This is intentionally consumptive so an event cannot be dispatched more
    // than once when a host redraws or changes the active document.
    [[nodiscard]] auto take_command() -> std::optional<std::string> {
        return std::exchange(pending_command_, std::nullopt);
    }

    void render(terminal::ScreenBuffer& buffer, Rect bounds) override {
        if (!WindowFrame::fits(buffer, bounds, 12, 4))
            return;
        WindowFrame::render(buffer, bounds, definition_.label, window_frame_appearance(*theme_));
        const auto content = WindowFrame::content_bounds(bounds);
        const int inner_width = std::max(0, content.width - 2);
        int row = content.y + 1;
        if (definition_.description && row < content.y + content.height) {
            write_line(buffer, content.x + 1, row++, inner_width, *definition_.description,
                       theme_->style(ThemeRole::muted_text));
            ++row;
        }
        for (std::size_t index = 0; index < buttons_.size() && row < content.y + content.height; ++index) {
            const int button_width = std::min(inner_width, buttons_[index].measure_width());
            buttons_[index].render(buffer, {content.x + 1, row++, button_width, 1}, index == selected_item_,
                                   theme_->style(ThemeRole::input));
            if (const auto& description = definition_.items[index].description;
                description && row < content.y + content.height) {
                write_line(buffer, content.x + 3, row++, std::max(0, inner_width - 2), *description,
                           theme_->style(ThemeRole::muted_text));
            }
        }
    }

  private:
    static void write_line(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text,
                           terminal::Style style) {
        if (width <= 0 || y < 0 || y >= buffer.height())
            return;
        const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
        for (int column = end; column < x + width; ++column)
            buffer.put(column, y, U' ', style);
    }

    appmeta::ScreenDefinition definition_;
    std::vector<Button> buttons_;
    std::size_t selected_item_{};
    std::optional<std::string> pending_command_;
    const Theme* theme_;
};

} // namespace vulpes::ui
