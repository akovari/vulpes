#include "vulpes/ui/workspace.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace vulpes::ui {
namespace {

void write(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text, terminal::Style style = {}) {
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

void write_menu(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view name, bool active,
                const Theme& theme) {
    write(buffer, x, y, width, name, active ? theme.style(ThemeRole::active_menu) : theme.style(ThemeRole::menu));
    buffer.put(x, y, terminal::first_code_point(name),
               active ? theme.style(ThemeRole::active_menu_mnemonic) : theme.style(ThemeRole::menu_mnemonic));
}

auto replace_argument(std::string text, std::string_view name, std::string_view value) -> std::string {
    const auto placeholder = "{" + std::string{name} + "}";
    std::size_t offset = 0;
    while ((offset = text.find(placeholder, offset)) != std::string::npos) {
        text.replace(offset, placeholder.size(), value);
        offset += value.size();
    }
    return text;
}

[[nodiscard]] auto lowercase_ascii(char32_t character) noexcept -> char32_t {
    return character >= U'A' && character <= U'Z' ? character - U'A' + U'a' : character;
}

} // namespace

Workspace::Workspace(WorkspaceText text, const Theme& theme)
    : text_{std::move(text)}, theme_{&theme}, windows_{theme, text_.workspace_document} {
}

void Workspace::set_database(std::string path, std::vector<db::TableSchema> tables) {
    prompt_.reset();
    close_confirmation_.reset();
    modal_ = Modal::none;
    submitted_value_.clear();
    windows_.dismiss_modal();
    windows_.reset_documents();
    database_path_ = std::move(path);
    set_tables(std::move(tables));
    set_status(replace_argument(replace_argument(text_.database_status, "path", database_path_), "count",
                                std::to_string(tables_.size())));
}

void Workspace::set_tables(std::vector<db::TableSchema> tables) {
    const auto selected_name = selected_table() == nullptr ? std::string{} : selected_table()->name;
    tables_ = std::move(tables);
    selected_table_ = 0;
    if (!selected_name.empty()) {
        if (const auto selected = std::ranges::find(tables_, selected_name, &db::TableSchema::name);
            selected != tables_.end()) {
            selected_table_ = static_cast<std::size_t>(std::distance(tables_.begin(), selected));
        }
    }
}

void Workspace::set_status(std::string status) {
    windows_.set_active_status(std::move(status));
}
auto Workspace::requested_path() const -> std::string {
    return submitted_value_;
}
auto Workspace::requested_command() const -> std::string {
    return submitted_value_;
}
auto Workspace::selected_table() const -> const db::TableSchema* {
    return selected_table_ < tables_.size() ? &tables_[selected_table_] : nullptr;
}
auto Workspace::active_document() const -> const Document& {
    return windows_.active();
}
auto Workspace::has_document(std::string_view id) const -> bool {
    return std::ranges::any_of(windows_.documents(), [id](const auto& document) { return document.id == id; });
}
auto Workspace::close_active_document() -> bool {
    return windows_.close_active();
}

void Workspace::open_browse(const db::TableSchema& table) {
    windows_.open({.id = "browse:" + table.name,
                   .title = replace_argument(text_.browse_document, "table", table.name),
                   .kind = DocumentKind::browse});
}

void Workspace::open_schema(const db::TableSchema& table) {
    windows_.open({.id = "schema:" + table.name,
                   .title = replace_argument(text_.schema_document, "table", table.name),
                   .kind = DocumentKind::schema});
}

void Workspace::open_sql_console() {
    windows_.open({.id = "sql", .title = text_.sql_document, .kind = DocumentKind::sql_console});
}

void Workspace::begin_path_prompt(Modal modal) {
    modal_ = modal;
    submitted_value_.clear();
    prompt_.emplace(modal == Modal::open ? text_.open_database_title : text_.create_database_title,
                    text_.path_instructions);
    windows_.show_modal(modal == Modal::open ? text_.open_database_title : text_.create_database_title);
}

void Workspace::begin_command_prompt() {
    modal_ = Modal::command;
    submitted_value_.clear();
    prompt_.emplace(text_.command_title, text_.command_instructions);
    windows_.show_modal(text_.command_title);
}

void Workspace::begin_close_confirmation() {
    if (!windows_.active().closable)
        return;
    close_confirmation_.emplace(
        text_.close_document_title, replace_argument(text_.close_document_message, "title", windows_.active().title),
        text_.close_document_confirm, text_.close_document_cancel, text_.close_document_instructions);
    windows_.show_modal(text_.close_document_title);
}

auto Workspace::activate_menu_item() -> WorkspaceResult {
    const auto selected = menu_selection_;
    const auto active_menu = menu_;
    menu_ = Menu::none;
    menu_selection_ = 0;

    switch (active_menu) {
    case Menu::file:
        if (selected == 0) {
            begin_path_prompt(Modal::open);
            return WorkspaceResult::redraw;
        }
        if (selected == 1) {
            begin_path_prompt(Modal::create);
            return WorkspaceResult::redraw;
        }
        return WorkspaceResult::quit;
    case Menu::database:
        if (selected == 0) {
            begin_path_prompt(Modal::open);
            return WorkspaceResult::redraw;
        }
        if (selected == 1) {
            begin_path_prompt(Modal::create);
            return WorkspaceResult::redraw;
        }
        if (selected == 2) {
            const auto* table = selected_table();
            if (table == nullptr) {
                set_status(text_.open_before_browse);
                return WorkspaceResult::redraw;
            }
            open_browse(*table);
            return WorkspaceResult::browse_table;
        }
        if (database_path_.empty()) {
            set_status(text_.open_before_sql);
            return WorkspaceResult::redraw;
        }
        open_sql_console();
        return WorkspaceResult::run_sql;
    case Menu::view:
        if (selected == 0 && selected_table_ > 0)
            --selected_table_;
        if (selected == 1 && selected_table_ + 1 < tables_.size())
            ++selected_table_;
        return WorkspaceResult::redraw;
    case Menu::window:
        if (selected == 0)
            static_cast<void>(windows_.handle(core::ActionId::workspace_next_document));
        else
            begin_close_confirmation();
        return WorkspaceResult::redraw;
    case Menu::help:
        set_status(text_.help_shortcuts);
        return WorkspaceResult::redraw;
    case Menu::none:
        return WorkspaceResult::unchanged;
    }
    return WorkspaceResult::unchanged;
}

auto Workspace::handle(core::ActionId action, const terminal::InputEvent& event) -> WorkspaceResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (action == core::ActionId::application_quit)
        return WorkspaceResult::quit;
    if (close_confirmation_) {
        const auto result = close_confirmation_->handle(event);
        if (result == ConfirmationResult::confirmed) {
            close_confirmation_.reset();
            windows_.dismiss_modal();
            return windows_.close_active() ? WorkspaceResult::redraw : WorkspaceResult::unchanged;
        }
        if (result == ConfirmationResult::cancelled) {
            close_confirmation_.reset();
            windows_.dismiss_modal();
            return WorkspaceResult::redraw;
        }
        return result == ConfirmationResult::unchanged ? WorkspaceResult::unchanged : WorkspaceResult::redraw;
    }
    const auto menu_from_mnemonic = [this, key] {
        if (key == nullptr || !key->alt)
            return Menu::none;
        static constexpr std::array menus{Menu::file, Menu::database, Menu::view, Menu::window, Menu::help};
        for (std::size_t index = 0; index < menus.size(); ++index) {
            const auto mnemonic = terminal::first_code_point(text_.menu_bar[index]);
            if (lowercase_ascii(key->character) == lowercase_ascii(mnemonic))
                return menus[index];
        }
        return Menu::none;
    };
    const auto menu_item_count = [this] {
        switch (menu_) {
        case Menu::file:
            return std::size_t{3};
        case Menu::database:
            return std::size_t{4};
        case Menu::view:
        case Menu::window:
            return std::size_t{2};
        case Menu::help:
            return std::size_t{1};
        case Menu::none:
            return std::size_t{0};
        }
        return std::size_t{0};
    };
    const auto move_menu = [this](int direction) {
        static constexpr std::array menus{Menu::file, Menu::database, Menu::view, Menu::window, Menu::help};
        const auto found = std::ranges::find(menus, menu_);
        const auto index = found == menus.end() ? 0 : static_cast<int>(std::distance(menus.begin(), found));
        const auto next = (index + direction + static_cast<int>(menus.size())) % static_cast<int>(menus.size());
        menu_ = menus[static_cast<std::size_t>(next)];
        menu_selection_ = 0;
    };

    if (prompt_) {
        if (key != nullptr && key->key == terminal::Key::escape) {
            prompt_.reset();
            modal_ = Modal::none;
            submitted_value_.clear();
            windows_.dismiss_modal();
            return WorkspaceResult::redraw;
        }
        const auto outcome = prompt_->handle(event);
        if (outcome == PromptResult::cancelled) {
            prompt_.reset();
            modal_ = Modal::none;
            submitted_value_.clear();
            windows_.dismiss_modal();
            return WorkspaceResult::redraw;
        }
        if (outcome != PromptResult::submitted)
            return outcome == PromptResult::unchanged ? WorkspaceResult::unchanged : WorkspaceResult::redraw;
        submitted_value_ = prompt_->value();
        prompt_.reset();
        const auto result = modal_ == Modal::open     ? WorkspaceResult::open_database
                            : modal_ == Modal::create ? WorkspaceResult::create_database
                                                      : WorkspaceResult::command;
        modal_ = Modal::none;
        windows_.dismiss_modal();
        return result;
    }
    if (action == core::ActionId::application_command_palette) {
        begin_command_prompt();
        return WorkspaceResult::redraw;
    }

    const auto mnemonic_menu = menu_from_mnemonic();
    if (menu_ != Menu::none) {
        if (mnemonic_menu != Menu::none) {
            menu_ = mnemonic_menu;
            menu_selection_ = 0;
            return WorkspaceResult::redraw;
        }
        if ((key != nullptr && key->key == terminal::Key::escape) || action == core::ActionId::application_back) {
            menu_ = Menu::none;
            menu_selection_ = 0;
            return WorkspaceResult::redraw;
        }
        if (action == core::ActionId::grid_previous_column) {
            move_menu(-1);
            return WorkspaceResult::redraw;
        }
        if (action == core::ActionId::grid_next_column) {
            move_menu(1);
            return WorkspaceResult::redraw;
        }
        const auto item_count = menu_item_count();
        if (action == core::ActionId::dataset_next && item_count > 0) {
            menu_selection_ = (menu_selection_ + 1) % item_count;
            return WorkspaceResult::redraw;
        }
        if (action == core::ActionId::dataset_previous && item_count > 0) {
            menu_selection_ = (menu_selection_ + item_count - 1) % item_count;
            return WorkspaceResult::redraw;
        }
        if (key != nullptr && key->key == terminal::Key::enter)
            return activate_menu_item();
        return WorkspaceResult::unchanged;
    }
    if (action == core::ActionId::workspace_close_document) {
        if (windows_.active().closable) {
            begin_close_confirmation();
            return WorkspaceResult::redraw;
        }
        return WorkspaceResult::unchanged;
    }
    if (windows_.handle(action))
        return WorkspaceResult::redraw;
    if (mnemonic_menu != Menu::none) {
        menu_ = mnemonic_menu;
        menu_selection_ = 0;
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::application_menu) {
        menu_ = Menu::file;
        menu_selection_ = 0;
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
    if (windows_.active().kind != DocumentKind::workspace)
        return WorkspaceResult::unchanged;
    if (action == core::ActionId::dataset_next && selected_table_ + 1 < tables_.size()) {
        ++selected_table_;
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::dataset_previous && selected_table_ > 0) {
        --selected_table_;
        return WorkspaceResult::redraw;
    }
    if (key && key->key == terminal::Key::enter && selected_table()) {
        const auto& table = *selected_table();
        open_browse(table);
        return WorkspaceResult::browse_table;
    }
    if (key && key->key == terminal::Key::f7 && !database_path_.empty()) {
        open_sql_console();
        return WorkspaceResult::run_sql;
    }
    return WorkspaceResult::unchanged;
}

void Workspace::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (bounds.width < 40 || bounds.height < 10)
        return;
    const auto& current_theme = *theme_;
    write(buffer, bounds.x, bounds.y, bounds.width, "", current_theme.style(ThemeRole::menu));
    static constexpr std::array menus{Menu::file, Menu::database, Menu::view, Menu::window, Menu::help};
    std::array<int, menus.size()> menu_positions{};
    int next_menu_x = bounds.x + 1;
    for (std::size_t index = 0; index < menus.size(); ++index) {
        menu_positions[index] = next_menu_x;
        const int available_width = bounds.x + bounds.width - next_menu_x;
        if (available_width <= 0)
            continue;
        const int label_width = std::min(terminal::text_width(text_.menu_bar[index]), available_width);
        write_menu(buffer, next_menu_x, bounds.y, label_width, text_.menu_bar[index], menu_ == menus[index],
                   current_theme);
        next_menu_x += label_width + 1;
    }
    windows_.render_tabs(buffer, {bounds.x, bounds.y + 1, bounds.width, 1});
    if (windows_.active().kind == DocumentKind::workspace) {
        write(buffer, bounds.x + 2, bounds.y + 3, bounds.width - 4, text_.title, current_theme.style(ThemeRole::title));
        if (database_path_.empty()) {
            write(buffer, bounds.x + 2, bounds.y + 5, bounds.width - 4, text_.no_database_open);
            write(buffer, bounds.x + 2, bounds.y + 6, bounds.width - 4, text_.home_shortcuts);
        } else {
            write(buffer, bounds.x + 2, bounds.y + 5, bounds.width - 4, database_path_);
            write(buffer, bounds.x + 2, bounds.y + 7, bounds.width - 4, text_.tables_and_views, {.bold = true});
            for (std::size_t index = 0; index < tables_.size() && static_cast<int>(index) < bounds.height - 11; ++index)
                write(buffer, bounds.x + 4, bounds.y + 8 + static_cast<int>(index), bounds.width - 8,
                      tables_[index].name + (tables_[index].is_view ? text_.view_suffix : ""),
                      index == selected_table_ ? current_theme.style(ThemeRole::selection)
                                               : current_theme.style(ThemeRole::text));
        }
    }
    if (menu_ != Menu::none) {
        std::span<const std::string> items;
        int menu_x = menu_positions[0];
        switch (menu_) {
        case Menu::file:
            items = text_.file_menu;
            break;
        case Menu::database:
            items = text_.database_menu;
            menu_x = menu_positions[1];
            break;
        case Menu::view:
            items = text_.view_menu;
            menu_x = menu_positions[2];
            break;
        case Menu::window:
            items = text_.window_menu;
            menu_x = menu_positions[3];
            break;
        case Menu::help:
            items = text_.help_menu;
            menu_x = menu_positions[4];
            break;
        case Menu::none:
            break;
        }
        const int menu_width = std::min(30, bounds.x + bounds.width - menu_x);
        if (menu_width >= 3) {
            const int menu_y = bounds.y + 1;
            buffer.put(menu_x, menu_y, U'+', current_theme.style(ThemeRole::popup));
            for (int column = 1; column < menu_width - 1; ++column)
                buffer.put(menu_x + column, menu_y, U'-', current_theme.style(ThemeRole::popup));
            buffer.put(menu_x + menu_width - 1, menu_y, U'+', current_theme.style(ThemeRole::popup));
            for (std::size_t index = 0; index < items.size(); ++index) {
                const int y = menu_y + 1 + static_cast<int>(index);
                const auto& style = index == menu_selection_ ? current_theme.style(ThemeRole::popup_selection)
                                                             : current_theme.style(ThemeRole::popup);
                buffer.put(menu_x, y, U'|', current_theme.style(ThemeRole::popup));
                write(buffer, menu_x + 1, y, menu_width - 2, items[index], style);
                buffer.put(menu_x + menu_width - 1, y, U'|', current_theme.style(ThemeRole::popup));
            }
            const int bottom = menu_y + static_cast<int>(items.size()) + 1;
            buffer.put(menu_x, bottom, U'+', current_theme.style(ThemeRole::popup));
            for (int column = 1; column < menu_width - 1; ++column)
                buffer.put(menu_x + column, bottom, U'-', current_theme.style(ThemeRole::popup));
            buffer.put(menu_x + menu_width - 1, bottom, U'+', current_theme.style(ThemeRole::popup));
        }
    }
    if (prompt_)
        prompt_->render(buffer, {bounds.x + (bounds.width - 60) / 2, bounds.y + (bounds.height - 5) / 2,
                                 std::min(60, bounds.width), 5});
    if (close_confirmation_) {
        constexpr int dialog_height = 6;
        const int dialog_width = std::min(60, bounds.width);
        close_confirmation_->render(buffer,
                                    {bounds.x + (bounds.width - dialog_width) / 2,
                                     bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
    }
    write(buffer, bounds.x, bounds.y + bounds.height - 1, bounds.width, "", current_theme.style(ThemeRole::menu));
    if (windows_.active_status().empty()) {
        const int status_end = bounds.x + bounds.width - 1;
        int status_x = bounds.x + 1;
        for (const auto& shortcut : text_.status_shortcuts) {
            if (status_x >= status_end)
                break;
            const int key_width = std::min(terminal::text_width(shortcut.key), status_end - status_x);
            write(buffer, status_x, bounds.y + bounds.height - 1, key_width, shortcut.key,
                  current_theme.style(ThemeRole::menu_mnemonic));
            status_x += key_width;
            if (status_x >= status_end)
                break;
            const int label_width = std::min(terminal::text_width(shortcut.label), status_end - status_x);
            write(buffer, status_x, bounds.y + bounds.height - 1, label_width, shortcut.label,
                  current_theme.style(ThemeRole::menu));
            status_x += label_width;
        }
    } else {
        write(buffer, bounds.x + 1, bounds.y + bounds.height - 1, bounds.width - 2, windows_.active_status(),
              current_theme.style(ThemeRole::menu));
    }
}

} // namespace vulpes::ui
