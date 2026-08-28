#include "vulpes/core/localization.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/schema_document.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("schema document renders localized field details and closes semantically", "[ui][schema]") {
    vulpes::core::Localizer messages{"en"};
    vulpes::ui::SchemaDocument document{
        {.name = "customers",
         .fields = {{.name = "id", .declared_type = "INTEGER", .nullable = false, .primary_key = true},
                    {.name = "name", .declared_type = "TEXT", .nullable = false}}},
        messages};
    vulpes::terminal::ScreenBuffer buffer{80, 12};

    document.render(buffer, {0, 0, 80, 12});
    CHECK(buffer.cell(0, 0).glyph == U'S');
    CHECK(buffer.cell(0, 1).glyph == U'i');
    CHECK(document.handle(vulpes::core::ActionId::dataset_next, {}) == vulpes::ui::DocumentResult::redraw);
    CHECK(document.handle(vulpes::core::ActionId::application_back, {}) == vulpes::ui::DocumentResult::close);
}
