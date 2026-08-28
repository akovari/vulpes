#include "vulpes/core/localization.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/browse_document.hpp"
#include "vulpes/ui/schema_document.hpp"
#include "vulpes/ui/sql_document.hpp"
#include "vulpes/ui/terminal_diagnostics.hpp"
#include "vulpes/ui/terminal_warning.hpp"
#include "vulpes/ui/workspace.hpp"
#include "vulpes/ui/workspace_text.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string_view>

namespace {

auto messages_for(std::string_view locale) -> vulpes::core::Localizer {
    vulpes::core::Localizer messages{std::string{locale}};
    if (locale.starts_with("cs"))
        messages.load_catalog_file(std::filesystem::path{VULPES_SOURCE_DIR} / "translations" / "cs.json");
    return messages;
}

} // namespace

TEST_CASE("interactive surfaces render across the supported size locale and palette matrix",
          "[ui][presentation][matrix]") {
    constexpr std::array sizes{vulpes::terminal::Size{40, 10}, vulpes::terminal::Size{80, 25},
                               vulpes::terminal::Size{160, 45}};
    constexpr std::array locales{std::string_view{"en"}, std::string_view{"cs-CZ"}};
    constexpr std::array palettes{vulpes::ui::ThemeName::midnight, vulpes::ui::ThemeName::high_contrast};

    for (const auto size : sizes) {
        for (const auto locale : locales) {
            for (const auto palette : palettes) {
                CAPTURE(size.width, size.height, locale, palette);
                auto messages = messages_for(locale);
                const auto& theme = vulpes::ui::theme(palette);
                vulpes::db::Database database{":memory:"};
                database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
                                 "INSERT INTO customer(name) VALUES ('Acme')");
                const auto schema = vulpes::db::inspect_schema(database);
                const auto bounds = vulpes::ui::Rect{0, 0, size.width, size.height};
                const auto document_bounds = vulpes::ui::Rect{0, 2, size.width, size.height - 3};
                vulpes::terminal::ScreenBuffer buffer{size.width, size.height};

                vulpes::ui::Workspace workspace{vulpes::ui::make_workspace_text(messages), theme};
                CHECK_NOTHROW(workspace.render(buffer, bounds));
                static_cast<void>(
                    workspace.handle(vulpes::core::ActionId::database_open, vulpes::terminal::KeyEvent{}));
                CHECK_NOTHROW(workspace.render(buffer, bounds));
                static_cast<void>(workspace.handle(vulpes::core::ActionId::application_back,
                                                   vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}));
                workspace.set_database("matrix.db", schema);
                static_cast<void>(workspace.handle(vulpes::core::ActionId::application_menu,
                                                   vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f10}));
                CHECK_NOTHROW(workspace.render(buffer, bounds));
                static_cast<void>(workspace.handle(vulpes::core::ActionId::dataset_next,
                                                   vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
                static_cast<void>(workspace.handle(vulpes::core::ActionId::dataset_next,
                                                   vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
                static_cast<void>(workspace.handle(vulpes::core::ActionId::none,
                                                   vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}));
                CHECK_NOTHROW(workspace.render(buffer, bounds));
                static_cast<void>(workspace.handle(vulpes::core::ActionId::application_back,
                                                   vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}));

                vulpes::ui::BrowseDocument browse{database, schema.front(), messages, theme};
                CHECK_NOTHROW(browse.render(buffer, bounds));
                CHECK_NOTHROW(browse.render(buffer, document_bounds));
                static_cast<void>(browse.handle(vulpes::core::ActionId::record_edit,
                                                vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f2}));
                CHECK_NOTHROW(browse.render(buffer, bounds));
                static_cast<void>(browse.handle(vulpes::core::ActionId::application_back,
                                                vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}));
                static_cast<void>(browse.handle(vulpes::core::ActionId::dataset_search,
                                                vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f3}));
                CHECK_NOTHROW(browse.render(buffer, bounds));
                static_cast<void>(browse.handle(vulpes::core::ActionId::application_back,
                                                vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}));
                static_cast<void>(browse.handle(vulpes::core::ActionId::record_delete,
                                                vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::delete_key}));
                CHECK_NOTHROW(browse.render(buffer, bounds));
                static_cast<void>(browse.handle(vulpes::core::ActionId::application_back,
                                                vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}));

                vulpes::ui::SqlDocument sql{database, messages, theme};
                static_cast<void>(sql.handle(vulpes::core::ActionId::none,
                                             vulpes::terminal::PasteEvent{.text = "SELECT *\nFROM customer"}));
                CHECK_NOTHROW(sql.render(buffer, bounds));
                CHECK_NOTHROW(sql.render(buffer, document_bounds));

                vulpes::ui::SchemaDocument schema_document{schema.front(), messages, theme};
                CHECK_NOTHROW(schema_document.render(buffer, bounds));
                CHECK_NOTHROW(schema_document.render(buffer, document_bounds));
                vulpes::ui::TerminalDiagnostics diagnostics{messages};
                static_cast<void>(
                    diagnostics.handle(vulpes::core::ActionId::none, vulpes::terminal::PasteEvent{.text = "matrix"}));
                CHECK_NOTHROW(diagnostics.render(buffer, bounds));
                CHECK_NOTHROW(vulpes::ui::render_terminal_warning(
                    buffer, {size.width, size.height},
                    messages.translate("terminal.minimum_size", {{"width", "40"}, {"height", "10"}})));
            }
        }
    }
}
