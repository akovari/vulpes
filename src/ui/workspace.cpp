#include "vulpes/ui/workspace.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <array>

namespace vulpes::ui {
namespace {

void write(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text, terminal::Style style = {}) {
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

Workspace::Workspace(std::string title, std::string open_label, std::string create_label, std::string path_instructions)
    : title_{std::move(title)}, open_label_{std::move(open_label)}, create_label_{std::move(create_label)},
      path_instructions_{std::move(path_instructions)} {
}

void Workspace::set_database(std::string path, std::vector<db::TableSchema> tables) {
    prompt_.reset();
    modal_ = Modal::none;
    database_path_ = std::move(path);
    tables_ = std::move(tables);
    selected_table_ = 0;
    status_ = database_path_ + " — " + std::to_string(tables_.size()) + " table(s) and view(s)";
}

void Workspace::set_status(std::string status) {
    status_ = std::move(status);
}
auto Workspace::requested_path() const -> std::string {
    return prompt_ ? std::string{prompt_->value()} : std::string{};
}
auto Workspace::selected_table() const -> const db::TableSchema* {
    return selected_table_ < tables_.size() ? &tables_[selected_table_] : nullptr;
}

void Workspace::begin_path_prompt(Modal modal) {
    modal_ = modal;
    prompt_.emplace(modal == Modal::open ? open_label_ : create_label_, path_instructions_);
}

auto Workspace::handle(core::ActionId action, const terminal::InputEvent& event) -> WorkspaceResult {
    if (prompt_) {
        const auto outcome = prompt_->handle(event);
        if (outcome == PromptResult::cancelled) {
            prompt_.reset();
            modal_ = Modal::none;
            return WorkspaceResult::redraw;
        }
        if (outcome != PromptResult::submitted)
            return outcome == PromptResult::unchanged ? WorkspaceResult::unchanged : WorkspaceResult::redraw;
        const auto result = modal_ == Modal::open ? WorkspaceResult::open_database : WorkspaceResult::create_database;
        modal_ = Modal::none;
        return result;
    }
    if (action == core::ActionId::application_quit)
        return WorkspaceResult::quit;
    if (action == core::ActionId::application_menu) {
        menu_open_ = !menu_open_;
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::database_open) {
        begin_path_prompt(Modal::open);
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::database_create) {
        begin_path_prompt(Modal::create);
        return WorkspaceResult::redraw;
    }
    if (menu_open_) {
        if (action == core::ActionId::dataset_next) {
            menu_selection_ = (menu_selection_ + 1) % 3;
            return WorkspaceResult::redraw;
        }
        if (action == core::ActionId::dataset_previous) {
            menu_selection_ = (menu_selection_ + 2) % 3;
            return WorkspaceResult::redraw;
        }
        if (const auto* key = std::get_if<terminal::KeyEvent>(&event); key && key->key == terminal::Key::enter) {
            menu_open_ = false;
            if (menu_selection_ == 0) {
                begin_path_prompt(Modal::open);
                return WorkspaceResult::redraw;
            }
            if (menu_selection_ == 1) {
                begin_path_prompt(Modal::create);
                return WorkspaceResult::redraw;
            }
            return WorkspaceResult::quit;
        }
        if (action == core::ActionId::application_back) {
            menu_open_ = false;
            return WorkspaceResult::redraw;
        }
        return WorkspaceResult::unchanged;
    }
    if (action == core::ActionId::dataset_next && selected_table_ + 1 < tables_.size()) {
        ++selected_table_;
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::dataset_previous && selected_table_ > 0) {
        --selected_table_;
        return WorkspaceResult::redraw;
    }
    if (const auto* key = std::get_if<terminal::KeyEvent>(&event);
        key && key->key == terminal::Key::enter && selected_table())
        return WorkspaceResult::browse_table;
    if (const auto* key = std::get_if<terminal::KeyEvent>(&event);
        key && key->key == terminal::Key::f7 && !database_path_.empty())
        return WorkspaceResult::run_sql;
    return WorkspaceResult::unchanged;
}

void Workspace::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (bounds.width < 40 || bounds.height < 10)
        return;
    write(buffer, bounds.x, bounds.y, bounds.width, " File  Database  View  Window  Help", {.reverse = true});
    write(buffer, bounds.x + 2, bounds.y + 2, bounds.width - 4, title_, {.bold = true});
    if (database_path_.empty()) {
        write(buffer, bounds.x + 2, bounds.y + 4, bounds.width - 4, "No database open.");
        write(buffer, bounds.x + 2, bounds.y + 5, bounds.width - 4,
              "Ctrl+O Open database   Ctrl+N Create database   F10 Menu");
    } else {
        write(buffer, bounds.x + 2, bounds.y + 4, bounds.width - 4, database_path_);
        write(buffer, bounds.x + 2, bounds.y + 6, bounds.width - 4, "Tables and views:", {.bold = true});
        for (std::size_t index = 0; index < tables_.size() && static_cast<int>(index) < bounds.height - 10; ++index)
            write(buffer, bounds.x + 4, bounds.y + 7 + static_cast<int>(index), bounds.width - 8,
                  tables_[index].name + (tables_[index].is_view ? " [view]" : ""),
                  {.reverse = index == selected_table_});
    }
    if (menu_open_) {
        static constexpr std::array<std::string_view, 3> items{"Open database", "Create database", "Exit"};
        for (std::size_t index = 0; index < items.size(); ++index)
            write(buffer, bounds.x, bounds.y + 1 + static_cast<int>(index), 24, items[index],
                  {.reverse = index == menu_selection_});
    }
    if (prompt_)
        prompt_->render(buffer, {bounds.x + (bounds.width - 60) / 2, bounds.y + (bounds.height - 5) / 2,
                                 std::min(60, bounds.width), 5});
    write(buffer, bounds.x, bounds.y + bounds.height - 1, bounds.width,
          status_.empty() ? "F10 Menu   Ctrl+O Open   Ctrl+N Create   Ctrl+C Exit" : status_, {.reverse = true});
}

} // namespace vulpes::ui
