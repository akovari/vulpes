#include "vulpes/core/application.hpp"
#include "vulpes/core/browse_controller.hpp"
#include "vulpes/core/command.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/console_terminal.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/confirmation_dialog.hpp"
#include "vulpes/ui/form.hpp"
#include "vulpes/ui/grid.hpp"
#include "vulpes/ui/text_prompt.hpp"
#include "vulpes/version.hpp"

#include <CLI/CLI.hpp>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void initialize_console_encoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

auto trim_ascii(std::string_view text) -> std::string_view {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        text.remove_suffix(1);
    return text;
}

auto lowercase_ascii(std::string_view text) -> std::string {
    std::string result{text};
    for (auto& character : result)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return result;
}

auto is_numeric_field(const vulpes::db::FieldSchema& field) -> bool {
    const auto type = lowercase_ascii(field.declared_type);
    return type.find("int") != std::string::npos || type.find("real") != std::string::npos ||
           type.find("floa") != std::string::npos || type.find("doub") != std::string::npos ||
           type.find("num") != std::string::npos || type.find("dec") != std::string::npos;
}

struct ParsedFilter {
    vulpes::model::FilterOperator comparison{vulpes::model::FilterOperator::equal};
    vulpes::db::Value value;
};

auto parse_filter(const vulpes::db::FieldSchema& field, std::string_view source) -> ParsedFilter {
    auto text = trim_ascii(source);
    ParsedFilter result;
    const auto consume_operator = [&](std::string_view prefix, vulpes::model::FilterOperator comparison) {
        if (text.starts_with(prefix)) {
            result.comparison = comparison;
            text = trim_ascii(text.substr(prefix.size()));
            return true;
        }
        return false;
    };
    static_cast<void>(consume_operator(">=", vulpes::model::FilterOperator::greater_equal) ||
                      consume_operator("<=", vulpes::model::FilterOperator::less_equal) ||
                      consume_operator("!=", vulpes::model::FilterOperator::not_equal) ||
                      consume_operator("<>", vulpes::model::FilterOperator::not_equal) ||
                      consume_operator(">", vulpes::model::FilterOperator::greater) ||
                      consume_operator("<", vulpes::model::FilterOperator::less) ||
                      consume_operator("=", vulpes::model::FilterOperator::equal));
    if (text.empty())
        throw vulpes::Error{vulpes::ErrorCategory::validation, "a filter value is required"};
    if (lowercase_ascii(text) == "null") {
        result.value = nullptr;
        return result;
    }
    if (!is_numeric_field(field)) {
        result.value = std::string{text};
        return result;
    }

    if (text.find_first_of(".eE") == std::string_view::npos) {
        std::int64_t value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error == std::errc{} && end == text.data() + text.size()) {
            result.value = value;
            return result;
        }
    } else {
        double value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error == std::errc{} && end == text.data() + text.size()) {
            result.value = value;
            return result;
        }
    }
    throw vulpes::Error{vulpes::ErrorCategory::validation, "invalid number for filter: " + field.name};
}

void browse(vulpes::db::Database& database, const vulpes::db::TableSchema& table,
            const vulpes::core::Localizer& messages) {
    vulpes::model::Dataset dataset{database, table};
    vulpes::core::BrowseController controller{dataset};
    vulpes::terminal::ConsoleTerminal terminal;
    auto terminal_size = terminal.size();
    if (terminal_size.width < 20 || terminal_size.height < 6) {
        throw vulpes::Error{vulpes::ErrorCategory::terminal, "terminal must be at least 20 columns by 6 rows"};
    }
    vulpes::terminal::ScreenBuffer previous{terminal_size.width, terminal_size.height};
    vulpes::terminal::ScreenBuffer current{terminal_size.width, terminal_size.height};
    vulpes::ui::Grid grid{dataset, table.name, messages.translate("browse.footer")};

    const auto update_terminal_size = [&] {
        const auto updated_size = terminal.size();
        if (updated_size.width == terminal_size.width && updated_size.height == terminal_size.height)
            return;
        terminal_size = updated_size;
        previous = vulpes::terminal::ScreenBuffer{terminal_size.width, terminal_size.height};
        current = vulpes::terminal::ScreenBuffer{terminal_size.width, terminal_size.height};
    };

    const auto edit_record = [&](vulpes::ui::FormMode mode) {
        vulpes::ui::RecordForm form{
            dataset, mode == vulpes::ui::FormMode::edit ? "Edit " + table.name : "New " + table.name, mode};
        for (;;) {
            update_terminal_size();
            current.clear();
            form.render(current, {0, 0, terminal_size.width, terminal_size.height});
            terminal.present(previous, current);
            previous = current;
            const auto result = form.handle(terminal.read_event());
            if (result == vulpes::ui::FormResult::saved || result == vulpes::ui::FormResult::cancelled)
                return;
        }
    };

    const auto prompt = [&](std::string label, auto&& apply) {
        vulpes::ui::TextPrompt text_prompt{std::move(label)};
        for (;;) {
            update_terminal_size();
            current.clear();
            grid.render(current, {0, 0, terminal_size.width, terminal_size.height});
            const int prompt_height = 5;
            text_prompt.render(current,
                               {0, (terminal_size.height - prompt_height) / 2, terminal_size.width, prompt_height});
            terminal.present(previous, current);
            previous = current;
            const auto result = text_prompt.handle(terminal.read_event());
            if (result == vulpes::ui::PromptResult::cancelled)
                return;
            if (result != vulpes::ui::PromptResult::submitted)
                continue;
            try {
                apply(text_prompt.value());
                return;
            } catch (const vulpes::Error& error) {
                text_prompt.set_error(error.what());
            }
        }
    };

    const auto confirm_delete = [&] {
        vulpes::ui::ConfirmationDialog dialog{messages.translate("browse.delete_title"),
                                              messages.translate("browse.delete_message", {{"table", table.name}}),
                                              messages.translate("dialog.delete"), messages.translate("dialog.cancel"),
                                              messages.translate("dialog.select")};
        for (;;) {
            update_terminal_size();
            current.clear();
            grid.render(current, {0, 0, terminal_size.width, terminal_size.height});
            constexpr int dialog_height = 6;
            const int dialog_width = (std::min)(terminal_size.width, 60);
            dialog.render(current, {(terminal_size.width - dialog_width) / 2,
                                    (terminal_size.height - dialog_height) / 2, dialog_width, dialog_height});
            terminal.present(previous, current);
            previous = current;
            const auto result = dialog.handle(terminal.read_event());
            if (result == vulpes::ui::ConfirmationResult::confirmed)
                return true;
            if (result == vulpes::ui::ConfirmationResult::cancelled)
                return false;
        }
    };

    std::optional<std::pair<std::string, vulpes::model::SortDirection>> sort;

    for (;;) {
        update_terminal_size();
        if (terminal_size.width < 20 || terminal_size.height < 6)
            continue;
        current.clear();
        grid.render(current, {0, 0, terminal_size.width, terminal_size.height});
        terminal.present(previous, current);
        previous = current;
        const auto event = terminal.read_event();
        if (const auto* key = std::get_if<vulpes::terminal::KeyEvent>(&event); key != nullptr) {
            if (key->key == vulpes::terminal::Key::f2 && dataset.is_editable()) {
                edit_record(vulpes::ui::FormMode::edit);
                continue;
            }
            if (key->key == vulpes::terminal::Key::insert_key && dataset.is_editable()) {
                edit_record(vulpes::ui::FormMode::insert);
                continue;
            }
            if (key->key == vulpes::terminal::Key::delete_key && dataset.is_editable() && dataset.current() &&
                confirm_delete()) {
                dataset.erase();
                continue;
            }
            if (key->key == vulpes::terminal::Key::f3) {
                prompt(messages.translate("browse.search_prompt"), [&](std::string_view text) {
                    if (text.empty())
                        dataset.clear_search();
                    else
                        dataset.search(text);
                });
                continue;
            }
            if (key->key == vulpes::terminal::Key::f4) {
                const auto* field = grid.selected_field();
                if (field != nullptr) {
                    prompt(messages.translate("browse.filter_prompt", {{"field", field->name}}),
                           [&](std::string_view text) {
                               if (text.empty()) {
                                   dataset.clear_filters();
                                   return;
                               }
                               const auto filter = parse_filter(*field, text);
                               dataset.where({field->name, filter.comparison, filter.value});
                           });
                }
                continue;
            }
            if (key->key == vulpes::terminal::Key::f5) {
                dataset.refresh();
                continue;
            }
            if (key->key == vulpes::terminal::Key::f6) {
                const auto* field = grid.selected_field();
                if (field != nullptr) {
                    const auto direction =
                        sort && sort->first == field->name && sort->second == vulpes::model::SortDirection::ascending
                            ? vulpes::model::SortDirection::descending
                            : vulpes::model::SortDirection::ascending;
                    dataset.order_by(field->name, direction);
                    sort = std::pair{field->name, direction};
                }
                continue;
            }
            if (key->key == vulpes::terminal::Key::left && grid.move_left())
                continue;
            if (key->key == vulpes::terminal::Key::right && grid.move_right())
                continue;
        }
        if (controller.handle(event) == vulpes::core::BrowseResult::close)
            return;
    }
}

void print_schema(const vulpes::db::TableSchema& table, const vulpes::core::Localizer& messages) {
    std::cout << messages.translate("schema.title", {{"name", table.name}}) << '\n';
    for (const auto& field : table.fields) {
        std::cout << "  " << field.name;
        if (!field.declared_type.empty())
            std::cout << " : " << field.declared_type;
        if (!field.nullable)
            std::cout << " [" << messages.translate("schema.not_null") << ']';
        if (field.primary_key)
            std::cout << " [" << messages.translate("schema.primary_key") << ']';
        if (field.unique)
            std::cout << " [" << messages.translate("schema.unique") << ']';
        if (field.generated)
            std::cout << " [" << messages.translate("schema.generated") << ']';
        std::cout << '\n';
    }
}

auto run(const std::filesystem::path& database_path, const std::optional<std::string>& table_name,
         const std::optional<std::string>& command_source, const std::string& locale,
         const std::vector<std::string>& catalog_paths) -> int {
    vulpes::db::Database database{database_path};
    vulpes::core::Localizer messages{locale};
    for (const auto& catalog_path : catalog_paths)
        messages.load_catalog_file(std::filesystem::path{catalog_path});
    vulpes::core::ApplicationRuntime application{database};
    std::cout << messages.translate("application.title") << " " << VULPES_VERSION << "\n\n";

    vulpes::core::Command command{.id = vulpes::core::CommandId::tables};
    if (table_name)
        command = {.id = vulpes::core::CommandId::browse, .arguments = {*table_name}};
    else if (command_source)
        command = vulpes::core::parse_command(*command_source);

    const auto response = application.execute(command);
    switch (response.outcome) {
    case vulpes::core::CommandOutcome::help:
        std::cout << messages.translate("command.help") << '\n';
        break;
    case vulpes::core::CommandOutcome::tables:
        std::cout << messages.translate("database.tables") << ":\n";
        for (const auto& table : response.tables)
            std::cout << "  " << table.name << (table.is_view ? " [view]" : "") << '\n';
        break;
    case vulpes::core::CommandOutcome::schema:
        print_schema(*response.table, messages);
        break;
    case vulpes::core::CommandOutcome::browse:
        browse(database, *response.table, messages);
        break;
    case vulpes::core::CommandOutcome::quit:
        break;
    case vulpes::core::CommandOutcome::unknown_command:
        std::cerr << messages.translate("application.unknown_command") << '\n';
        return 1;
    case vulpes::core::CommandOutcome::invalid_arguments:
        std::cerr << messages.translate("error.invalid_command_arguments",
                                        {{"command", std::string{vulpes::core::action_id(response.command)}}})
                  << '\n';
        return 1;
    case vulpes::core::CommandOutcome::table_not_found:
        std::cerr << messages.translate("error.unknown_table", {{"name", command.arguments.front()}}) << '\n';
        return 1;
    }
    return 0;
}

} // namespace

#ifdef _WIN32
auto wmain(int argc, wchar_t** argv) -> int {
#else
auto main(int argc, char** argv) -> int {
#endif
    initialize_console_encoding();
    try {
        CLI::App app{"A keyboard-first RAD environment for local SQLite applications"};
        bool version = false;
        std::string database_argument;
        std::string table_name;
        std::string command_source;
        std::string locale{"en"};
        std::vector<std::string> catalog_paths;
        app.add_flag("--version", version, "Show Vulpes version and exit");
        app.add_option("database", database_argument, "SQLite database path");
        const auto table_option = app.add_option("--table", table_name, "Browse one table or view");
        const auto command_option = app.add_option("--command", command_source, "Run one Vulpes command and exit");
        table_option->excludes(command_option);
        app.add_option("--locale", locale, "BCP-47 locale for user-interface messages");
        app.add_option("--catalog", catalog_paths, "UTF-8 JSON message catalog; may be repeated");
        app.set_help_flag("-h,--help", "Show this help and exit");
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& error) {
            return app.exit(error);
        }

        if (version) {
            std::cout << "Vulpes " << VULPES_VERSION << '\n';
            return 0;
        }
        if (database_argument.empty()) {
            std::cout << app.help();
            return 0;
        }

        return run(std::filesystem::path{database_argument},
                   table_name.empty() ? std::nullopt : std::optional{table_name},
                   command_source.empty() ? std::nullopt : std::optional{command_source}, locale, catalog_paths);
    } catch (const vulpes::Error& error) {
        std::cerr << "vulpes: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "vulpes: unexpected error: " << error.what() << '\n';
        return 1;
    }
}
