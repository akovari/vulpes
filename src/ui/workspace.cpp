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

constexpr terminal::Style menu_style{.foreground = {230, 242, 255}, .background = {0, 45, 110}, .bold = true};
constexpr terminal::Style accent_style{
    .foreground = {255, 220, 90}, .background = {0, 45, 110}, .bold = true, .underline = true};
constexpr terminal::Style title_style{.foreground = {90, 210, 255}, .background = {0, 0, 0}, .bold = true};
constexpr terminal::Style selected_style{.foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true};
constexpr terminal::Style popup_style{.foreground = {220, 235, 255}, .background = {0, 25, 65}};
constexpr terminal::Style popup_selected_style{.foreground = {0, 0, 0}, .background = {255, 220, 90}, .bold = true};
constexpr terminal::Style active_menu_style{.foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true};
constexpr terminal::Style active_accent_style{
    .foreground = {0, 0, 0}, .background = {95, 220, 255}, .bold = true, .underline = true};

void write_menu(terminal::ScreenBuffer& buffer, int x, int y, std::string_view name, bool active) {
    write(buffer, x, y, static_cast<int>(name.size()), name, active ? active_menu_style : menu_style);
    buffer.put(x, y, static_cast<char32_t>(name.front()), active ? active_accent_style : accent_style);
}

} // namespace

Workspace::Workspace(std::string title, std::string open_label, std::string create_label, std::string path_instructions)
    : title_{std::move(title)}, open_label_{std::move(open_label)}, create_label_{std::move(create_label)},
      path_instructions_{std::move(path_instructions)} {
}

void Workspace::set_database(std::string path, std::vector<db::TableSchema> tables) {
    prompt_.reset();
    modal_ = Modal::none;
    windows_.dismiss_modal();
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
auto Workspace::active_document() const -> const Document& {
    return windows_.active();
}
auto Workspace::has_document(std::string_view id) const -> bool {
    return std::ranges::any_of(windows_.documents(), [id](const auto& document) { return document.id == id; });
}
auto Workspace::close_active_document() -> bool {
    return windows_.close_active();
}

void Workspace::begin_path_prompt(Modal modal) {
    modal_ = modal;
    prompt_.emplace(modal == Modal::open ? open_label_ : create_label_, path_instructions_);
    windows_.show_modal(modal == Modal::open ? open_label_ : create_label_);
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
                status_ = "Open a database before browsing a table.";
                return WorkspaceResult::redraw;
            }
            windows_.open(
                {.id = "browse:" + table->name, .title = "Browse " + table->name, .kind = DocumentKind::browse});
            return WorkspaceResult::browse_table;
        }
        if (database_path_.empty()) {
            status_ = "Open a database before using the SQL console.";
            return WorkspaceResult::redraw;
        }
        windows_.open({.id = "sql", .title = "SQL", .kind = DocumentKind::sql_console});
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
            static_cast<void>(windows_.close_active());
        return WorkspaceResult::redraw;
    case Menu::help:
        status_ = "F10 menu  Alt+F File  Ctrl+O open  Ctrl+N create  Ctrl+Tab next tab  Ctrl+W close tab";
        return WorkspaceResult::redraw;
    case Menu::none:
        return WorkspaceResult::unchanged;
    }
    return WorkspaceResult::unchanged;
}

auto Workspace::handle(core::ActionId action, const terminal::InputEvent& event) -> WorkspaceResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    const auto menu_from_mnemonic = [key] {
        if (key == nullptr || !key->alt)
            return Menu::none;
        switch (key->character) {
        case U'f':
        case U'F':
            return Menu::file;
        case U'd':
        case U'D':
            return Menu::database;
        case U'v':
        case U'V':
            return Menu::view;
        case U'w':
        case U'W':
            return Menu::window;
        case U'h':
        case U'H':
            return Menu::help;
        default:
            return Menu::none;
        }
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
            windows_.dismiss_modal();
            return WorkspaceResult::redraw;
        }
        const auto outcome = prompt_->handle(event);
        if (outcome == PromptResult::cancelled) {
            prompt_.reset();
            modal_ = Modal::none;
            windows_.dismiss_modal();
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
        if (windows_.close_active())
            return WorkspaceResult::redraw;
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
        windows_.open({.id = "browse:" + table.name, .title = "Browse " + table.name, .kind = DocumentKind::browse});
        return WorkspaceResult::browse_table;
    }
    if (key && key->key == terminal::Key::f7 && !database_path_.empty()) {
        windows_.open({.id = "sql", .title = "SQL", .kind = DocumentKind::sql_console});
        return WorkspaceResult::run_sql;
    }
    return WorkspaceResult::unchanged;
}

void Workspace::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (bounds.width < 40 || bounds.height < 10)
        return;
    write(buffer, bounds.x, bounds.y, bounds.width, "", menu_style);
    write_menu(buffer, bounds.x + 1, bounds.y, "File", menu_ == Menu::file);
    write_menu(buffer, bounds.x + 7, bounds.y, "Database", menu_ == Menu::database);
    write_menu(buffer, bounds.x + 17, bounds.y, "View", menu_ == Menu::view);
    write_menu(buffer, bounds.x + 23, bounds.y, "Window", menu_ == Menu::window);
    write_menu(buffer, bounds.x + 32, bounds.y, "Help", menu_ == Menu::help);
    windows_.render_tabs(buffer, {bounds.x, bounds.y + 1, bounds.width, 1});
    if (windows_.active().kind == DocumentKind::workspace) {
        write(buffer, bounds.x + 2, bounds.y + 3, bounds.width - 4, title_, title_style);
        if (database_path_.empty()) {
            write(buffer, bounds.x + 2, bounds.y + 5, bounds.width - 4, "No database open.");
            write(buffer, bounds.x + 2, bounds.y + 6, bounds.width - 4,
                  "Ctrl+O Open database   Ctrl+N Create database   F10 Menu");
        } else {
            write(buffer, bounds.x + 2, bounds.y + 5, bounds.width - 4, database_path_);
            write(buffer, bounds.x + 2, bounds.y + 7, bounds.width - 4, "Tables and views:", {.bold = true});
            for (std::size_t index = 0; index < tables_.size() && static_cast<int>(index) < bounds.height - 11; ++index)
                write(buffer, bounds.x + 4, bounds.y + 8 + static_cast<int>(index), bounds.width - 8,
                      tables_[index].name + (tables_[index].is_view ? " [view]" : ""),
                      index == selected_table_ ? selected_style : terminal::Style{});
        }
    }
    if (menu_ != Menu::none) {
        static constexpr std::array<std::string_view, 3> file_items{"Open database", "Create database", "Exit"};
        static constexpr std::array<std::string_view, 4> database_items{"Open database", "Create database",
                                                                        "Browse selected table", "SQL console"};
        static constexpr std::array<std::string_view, 2> view_items{"Previous table", "Next table"};
        static constexpr std::array<std::string_view, 2> window_items{"Next document", "Close document"};
        static constexpr std::array<std::string_view, 1> help_items{"Keyboard shortcuts"};
        std::span<const std::string_view> items;
        int menu_x = bounds.x + 1;
        switch (menu_) {
        case Menu::file:
            items = file_items;
            break;
        case Menu::database:
            items = database_items;
            menu_x = bounds.x + 7;
            break;
        case Menu::view:
            items = view_items;
            menu_x = bounds.x + 17;
            break;
        case Menu::window:
            items = window_items;
            menu_x = bounds.x + 23;
            break;
        case Menu::help:
            items = help_items;
            menu_x = bounds.x + 32;
            break;
        case Menu::none:
            break;
        }
        const int menu_width = std::min(30, bounds.x + bounds.width - menu_x);
        const int menu_y = bounds.y + 1;
        buffer.put(menu_x, menu_y, U'+', popup_style);
        for (int column = 1; column < menu_width - 1; ++column)
            buffer.put(menu_x + column, menu_y, U'-', popup_style);
        buffer.put(menu_x + menu_width - 1, menu_y, U'+', popup_style);
        for (std::size_t index = 0; index < items.size(); ++index) {
            const int y = menu_y + 1 + static_cast<int>(index);
            const auto style = index == menu_selection_ ? popup_selected_style : popup_style;
            buffer.put(menu_x, y, U'|', popup_style);
            write(buffer, menu_x + 1, y, menu_width - 2, items[index], style);
            buffer.put(menu_x + menu_width - 1, y, U'|', popup_style);
        }
        const int bottom = menu_y + static_cast<int>(items.size()) + 1;
        buffer.put(menu_x, bottom, U'+', popup_style);
        for (int column = 1; column < menu_width - 1; ++column)
            buffer.put(menu_x + column, bottom, U'-', popup_style);
        buffer.put(menu_x + menu_width - 1, bottom, U'+', popup_style);
    }
    if (prompt_)
        prompt_->render(buffer, {bounds.x + (bounds.width - 60) / 2, bounds.y + (bounds.height - 5) / 2,
                                 std::min(60, bounds.width), 5});
    write(buffer, bounds.x, bounds.y + bounds.height - 1, bounds.width, "", menu_style);
    if (status_.empty()) {
        write(buffer, bounds.x + 1, bounds.y + bounds.height - 1, 3, "F10", accent_style);
        write(buffer, bounds.x + 4, bounds.y + bounds.height - 1, 6, " Menu  ", menu_style);
        write(buffer, bounds.x + 10, bounds.y + bounds.height - 1, 6, "Ctrl+O", accent_style);
        write(buffer, bounds.x + 16, bounds.y + bounds.height - 1, 7, " Open  ", menu_style);
        write(buffer, bounds.x + 23, bounds.y + bounds.height - 1, 6, "Ctrl+N", accent_style);
        write(buffer, bounds.x + 29, bounds.y + bounds.height - 1, 9, " Create  ", menu_style);
        write(buffer, bounds.x + 38, bounds.y + bounds.height - 1, 6, "Ctrl+C", accent_style);
        write(buffer, bounds.x + 44, bounds.y + bounds.height - 1, 5, " Exit", menu_style);
    } else {
        write(buffer, bounds.x + 1, bounds.y + bounds.height - 1, bounds.width - 2, status_, menu_style);
    }
}

} // namespace vulpes::ui
