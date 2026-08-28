#include "vulpes/ui/workspace.hpp"

#include "vulpes/terminal/unicode.hpp"
#include "vulpes/ui/status_bar.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
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
    const auto label = " " + std::string{name} + " ";
    write(buffer, x, y, width, label, active ? theme.style(ThemeRole::active_menu) : theme.style(ThemeRole::menu));
    buffer.put(x + 1, y, terminal::first_code_point(name),
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

auto path_text(const std::filesystem::path& path) -> std::string {
    const auto encoded = path.generic_u8string();
    return {encoded.begin(), encoded.end()};
}

} // namespace

Workspace::Workspace(WorkspaceText text, const Theme& theme)
    : text_{std::move(text)}, theme_{&theme}, windows_{theme, text_.workspace_document} {
}

void Workspace::set_database(std::string path, std::vector<db::TableSchema> tables, bool read_only) {
    prompt_.reset();
    directory_browser_.reset();
    close_confirmation_.reset();
    modal_ = Modal::none;
    submitted_value_.clear();
    windows_.dismiss_modal();
    windows_.reset_documents();
    database_path_ = std::move(path);
    database_read_only_ = read_only;
    set_tables(std::move(tables));
    auto status = replace_argument(replace_argument(text_.database_status, "path", database_path_), "count",
                                   std::to_string(tables_.size()));
    if (database_read_only_)
        status += text_.read_only_suffix;
    set_status(std::move(status));
}

void Workspace::set_recent_databases(std::vector<std::string> paths) {
    recent_databases_ = std::move(paths);
    if (selected_recent_database_ >= recent_databases_.size())
        selected_recent_database_ = 0;
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
    const auto& title = modal == Modal::open             ? text_.open_database_title
                        : modal == Modal::open_read_only ? text_.open_read_only_database_title
                                                         : text_.create_database_title;
    prompt_.emplace(title, text_.path_instructions, std::string{}, *theme_);
    windows_.show_modal(title);
}

void Workspace::begin_directory_browser() {
    submitted_value_.clear();
    std::error_code error;
    auto directory = std::filesystem::current_path(error);
    if (error)
        directory.clear();
    directory_browser_.emplace(std::move(directory), text_.directory_browser_title,
                               text_.directory_browser_instructions, text_.directory_browser_parent, *theme_);
    windows_.show_modal(text_.directory_browser_title);
}

void Workspace::begin_command_prompt() {
    modal_ = Modal::command;
    submitted_value_.clear();
    prompt_.emplace(text_.command_title, text_.command_instructions, std::string{}, *theme_);
    windows_.show_modal(text_.command_title);
}

void Workspace::begin_close_confirmation() {
    if (!windows_.active().closable)
        return;
    close_confirmation_.emplace(
        text_.close_document_title, replace_argument(text_.close_document_message, "title", windows_.active().title),
        text_.close_document_confirm, text_.close_document_cancel, text_.close_document_instructions, *theme_);
    windows_.show_modal(text_.close_document_title);
}

auto Workspace::menu_item_enabled(Menu menu, std::size_t item) const noexcept -> bool {
    if (menu == Menu::database && item >= 4)
        return !database_path_.empty();
    if (menu == Menu::view)
        return item == 0 ? selected_table_ > 0 : item == 1 && selected_table_ + 1 < tables_.size();
    if (menu == Menu::window)
        return item == 0 ? windows_.documents().size() > 1 : item == 1 && windows_.active().closable;
    return true;
}

auto Workspace::activate_menu_item() -> WorkspaceResult {
    const auto selected = menu_selection_;
    const auto active_menu = menu_;
    if (!menu_item_enabled(active_menu, selected))
        return WorkspaceResult::unchanged;
    menu_ = Menu::none;
    menu_selection_ = 0;

    switch (active_menu) {
    case Menu::file:
        if (selected == 0) {
            begin_path_prompt(Modal::open);
            return WorkspaceResult::redraw;
        }
        if (selected == 1) {
            begin_path_prompt(Modal::open_read_only);
            return WorkspaceResult::redraw;
        }
        if (selected == 2) {
            begin_directory_browser();
            return WorkspaceResult::redraw;
        }
        if (selected == 3) {
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
            begin_path_prompt(Modal::open_read_only);
            return WorkspaceResult::redraw;
        }
        if (selected == 2) {
            begin_directory_browser();
            return WorkspaceResult::redraw;
        }
        if (selected == 3) {
            begin_path_prompt(Modal::create);
            return WorkspaceResult::redraw;
        }
        if (selected == 4) {
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
    if (directory_browser_) {
        const auto result = directory_browser_->handle(event);
        if (result == DirectoryBrowserResult::selected) {
            submitted_value_ = path_text(*directory_browser_->selected_path());
            directory_browser_.reset();
            windows_.dismiss_modal();
            return WorkspaceResult::open_database;
        }
        if (result == DirectoryBrowserResult::cancelled) {
            directory_browser_.reset();
            windows_.dismiss_modal();
            return WorkspaceResult::redraw;
        }
        return result == DirectoryBrowserResult::unchanged ? WorkspaceResult::unchanged : WorkspaceResult::redraw;
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
            return std::size_t{5};
        case Menu::database:
            return std::size_t{6};
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
    const auto select_first_enabled = [this, &menu_item_count] {
        const auto count = menu_item_count();
        for (std::size_t index = 0; index < count; ++index) {
            if (menu_item_enabled(menu_, index)) {
                menu_selection_ = index;
                return;
            }
        }
        menu_selection_ = 0;
    };
    const auto move_menu_selection = [this, &menu_item_count](int direction) {
        const auto count = menu_item_count();
        if (count == 0)
            return;
        for (std::size_t attempt = 0; attempt < count; ++attempt) {
            menu_selection_ = direction > 0 ? (menu_selection_ + 1) % count : (menu_selection_ + count - 1) % count;
            if (menu_item_enabled(menu_, menu_selection_))
                return;
        }
    };
    const auto move_menu = [this, &select_first_enabled](int direction) {
        static constexpr std::array menus{Menu::file, Menu::database, Menu::view, Menu::window, Menu::help};
        const auto found = std::ranges::find(menus, menu_);
        const auto index = found == menus.end() ? 0 : static_cast<int>(std::distance(menus.begin(), found));
        const auto next = (index + direction + static_cast<int>(menus.size())) % static_cast<int>(menus.size());
        menu_ = menus[static_cast<std::size_t>(next)];
        select_first_enabled();
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
        const auto result = modal_ == Modal::open             ? WorkspaceResult::open_database
                            : modal_ == Modal::open_read_only ? WorkspaceResult::open_database_read_only
                            : modal_ == Modal::create         ? WorkspaceResult::create_database
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
            select_first_enabled();
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
            move_menu_selection(1);
            return WorkspaceResult::redraw;
        }
        if (action == core::ActionId::dataset_previous && item_count > 0) {
            move_menu_selection(-1);
            return WorkspaceResult::redraw;
        }
        if (key != nullptr && key->key == terminal::Key::character && !key->ctrl && !key->alt) {
            std::span<const std::string> items;
            switch (menu_) {
            case Menu::file:
                items = text_.file_menu;
                break;
            case Menu::database:
                items = text_.database_menu;
                break;
            case Menu::view:
                items = text_.view_menu;
                break;
            case Menu::window:
                items = text_.window_menu;
                break;
            case Menu::help:
                items = text_.help_menu;
                break;
            case Menu::none:
                break;
            }
            std::vector<std::size_t> matches;
            for (std::size_t index = 0; index < items.size(); ++index) {
                if (menu_item_enabled(menu_, index) &&
                    lowercase_ascii(terminal::first_code_point(items[index])) == lowercase_ascii(key->character)) {
                    matches.push_back(index);
                }
            }
            if (matches.size() == 1) {
                menu_selection_ = matches.front();
                return activate_menu_item();
            }
            if (!matches.empty()) {
                const auto next =
                    std::ranges::find_if(matches, [&](std::size_t index) { return index > menu_selection_; });
                menu_selection_ = next == matches.end() ? matches.front() : *next;
                return WorkspaceResult::redraw;
            }
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
        select_first_enabled();
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::application_menu) {
        menu_ = Menu::file;
        select_first_enabled();
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::database_open) {
        begin_path_prompt(Modal::open);
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::database_open_read_only) {
        begin_path_prompt(Modal::open_read_only);
        return WorkspaceResult::redraw;
    }
    if (action == core::ActionId::database_create) {
        begin_path_prompt(Modal::create);
        return WorkspaceResult::redraw;
    }
    if (windows_.active().kind != DocumentKind::workspace)
        return WorkspaceResult::unchanged;
    if (database_path_.empty()) {
        if (action == core::ActionId::dataset_next && selected_recent_database_ + 1 < recent_databases_.size()) {
            ++selected_recent_database_;
            return WorkspaceResult::redraw;
        }
        if (action == core::ActionId::dataset_previous && selected_recent_database_ > 0) {
            --selected_recent_database_;
            return WorkspaceResult::redraw;
        }
        if (key && key->key == terminal::Key::enter && selected_recent_database_ < recent_databases_.size()) {
            submitted_value_ = recent_databases_[selected_recent_database_];
            return WorkspaceResult::open_database;
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
        const int label_width = std::min(terminal::text_width(text_.menu_bar[index]) + 2, available_width);
        write_menu(buffer, next_menu_x, bounds.y, label_width, text_.menu_bar[index], menu_ == menus[index],
                   current_theme);
        next_menu_x += label_width + 1;
    }
    windows_.render_tabs(buffer, {bounds.x, bounds.y + 1, bounds.width, 1});
    if (windows_.active().kind == DocumentKind::workspace) {
        for (int row = bounds.y + 2; row < bounds.y + bounds.height - 1; ++row)
            write(buffer, bounds.x, row, bounds.width, "", current_theme.style(ThemeRole::desktop));
        const Rect panel{bounds.x + 1, bounds.y + 3, bounds.width - 2, bounds.height - 5};
        WindowFrame::render(buffer, panel, text_.title, window_frame_appearance(current_theme));
        const auto content = WindowFrame::content_bounds(panel);
        if (database_path_.empty()) {
            write(buffer, content.x + 1, content.y + 1, content.width - 2, text_.no_database_open,
                  current_theme.style(ThemeRole::text));
            write(buffer, content.x + 1, content.y + 2, content.width - 2, text_.home_shortcuts,
                  current_theme.style(ThemeRole::muted_text));
            if (!recent_databases_.empty()) {
                write(buffer, content.x + 1, content.y + 4, content.width - 2, text_.recent_databases,
                      current_theme.style(ThemeRole::title));
                const int first_recent_row = content.y + 5;
                const int final_content_row = content.y + content.height;
                for (std::size_t index = 0;
                     index < recent_databases_.size() && first_recent_row + static_cast<int>(index) < final_content_row;
                     ++index) {
                    write(buffer, content.x + 2, first_recent_row + static_cast<int>(index), content.width - 4,
                          " " + recent_databases_[index],
                          index == selected_recent_database_ ? current_theme.style(ThemeRole::selection)
                                                             : current_theme.style(ThemeRole::text));
                }
            }
        } else {
            write(buffer, content.x + 1, content.y, content.width - 2,
                  (database_read_only_ ? "▣ " : "▰ ") + database_path_, current_theme.style(ThemeRole::muted_text));
            write(buffer, content.x + 1, content.y + 2, content.width - 2, text_.tables_and_views,
                  current_theme.style(ThemeRole::title));
            for (std::size_t index = 0;
                 index < tables_.size() && content.y + 3 + static_cast<int>(index) < content.y + content.height;
                 ++index)
                write(buffer, content.x + 2, content.y + 3 + static_cast<int>(index), content.width - 4,
                      " " + tables_[index].name + (tables_[index].is_view ? text_.view_suffix : ""),
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
        const auto shortcuts = [&]() -> std::span<const std::string> {
            switch (menu_) {
            case Menu::file:
                return text_.file_menu_shortcuts;
            case Menu::database:
                return text_.database_menu_shortcuts;
            case Menu::view:
                return text_.view_menu_shortcuts;
            case Menu::window:
                return text_.window_menu_shortcuts;
            case Menu::help:
                return text_.help_menu_shortcuts;
            case Menu::none:
                return {};
            }
            return {};
        }();
        int desired_width = 18;
        for (std::size_t index = 0; index < items.size(); ++index)
            desired_width = std::max(desired_width,
                                     terminal::text_width(items[index]) + terminal::text_width(shortcuts[index]) + 6);
        const bool separated = menu_ == Menu::file || menu_ == Menu::database;
        const int menu_height = static_cast<int>(items.size()) + 2 + (separated ? 1 : 0);
        const int maximum_width = bounds.x + bounds.width - 1;
        const int menu_width = std::min(desired_width, bounds.width - 1);
        menu_x = std::clamp(menu_x, bounds.x, maximum_width - menu_width);
        const int menu_y = bounds.y + 1;
        if (menu_width >= 10 && menu_y + menu_height <= bounds.y + bounds.height) {
            const WindowFrameAppearance appearance{.content = current_theme.style(ThemeRole::popup),
                                                   .border = current_theme.style(ThemeRole::popup),
                                                   .title = current_theme.style(ThemeRole::popup),
                                                   .shadow = current_theme.style(ThemeRole::shadow),
                                                   .drop_shadow = true};
            WindowFrame::render(buffer, {menu_x, menu_y, menu_width, menu_height}, "", appearance);
            int row = menu_y + 1;
            for (std::size_t index = 0; index < items.size(); ++index) {
                if (separated && index == 4) {
                    buffer.put(menu_x, row, U'├', current_theme.style(ThemeRole::popup));
                    for (int column = 1; column < menu_width - 1; ++column)
                        buffer.put(menu_x + column, row, U'─', current_theme.style(ThemeRole::popup));
                    buffer.put(menu_x + menu_width - 1, row, U'┤', current_theme.style(ThemeRole::popup));
                    ++row;
                }
                const bool selected = index == menu_selection_;
                const auto role = !menu_item_enabled(menu_, index) ? ThemeRole::disabled
                                  : selected                       ? ThemeRole::popup_selection
                                                                   : ThemeRole::popup;
                const auto& style = current_theme.style(role);
                write(buffer, menu_x + 1, row, menu_width - 2, "", style);
                buffer.put(menu_x + 1, row, selected ? U'►' : U' ', style);
                write(buffer, menu_x + 3, row, menu_width - 6, items[index], style);
                if (!items[index].empty()) {
                    auto mnemonic_style = style;
                    mnemonic_style.underline = true;
                    buffer.put(menu_x + 3, row, terminal::first_code_point(items[index]), mnemonic_style);
                }
                const int shortcut_width = terminal::text_width(shortcuts[index]);
                if (shortcut_width > 0)
                    write(buffer, menu_x + menu_width - shortcut_width - 2, row, shortcut_width, shortcuts[index],
                          style);
                ++row;
            }
        }
    }
    if (prompt_) {
        const int dialog_width = std::min(60, bounds.width);
        prompt_->render(buffer, {bounds.x + (bounds.width - dialog_width) / 2, bounds.y + (bounds.height - 5) / 2,
                                 dialog_width, 5});
    }
    if (directory_browser_) {
        const int dialog_height = std::min(20, bounds.height - 2);
        const int dialog_width = std::min(60, bounds.width);
        directory_browser_->render(buffer,
                                   {bounds.x + (bounds.width - dialog_width) / 2,
                                    bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
    }
    if (close_confirmation_) {
        constexpr int dialog_height = 6;
        const int dialog_width = std::min(60, bounds.width);
        close_confirmation_->render(buffer,
                                    {bounds.x + (bounds.width - dialog_width) / 2,
                                     bounds.y + (bounds.height - dialog_height) / 2, dialog_width, dialog_height});
    }
    StatusBar::render(buffer, {bounds.x, bounds.y + bounds.height - 1, bounds.width, 1}, current_theme,
                      windows_.active_status(), text_.status_shortcuts);
}

} // namespace vulpes::ui
