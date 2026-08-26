#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/input.hpp"
#include "vulpes/terminal/screen_buffer.hpp"
#include "vulpes/ui/form.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

auto form_dataset(vulpes::db::Database& database) -> vulpes::model::Dataset {
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL, balance REAL, active INTEGER);"
                     "INSERT INTO customer VALUES (1, 'Acme', 10.5, 1)");
    return {database, vulpes::db::inspect_schema(database).front()};
}

} // namespace

TEST_CASE("generated form infers controls and persists keyboard edits", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    auto dataset = form_dataset(database);
    vulpes::ui::RecordForm form{dataset, "Customer", vulpes::ui::FormMode::edit, "F8 Save"};

    REQUIRE(form.fields().size() == 4);
    CHECK(form.fields()[0].kind == vulpes::ui::FormFieldKind::read_only);
    CHECK(form.fields()[1].kind == vulpes::ui::FormFieldKind::text);
    CHECK(form.fields()[2].kind == vulpes::ui::FormFieldKind::number);
    CHECK(form.fields()[3].kind == vulpes::ui::FormFieldKind::checkbox);

    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::backspace}));
    static_cast<void>(
        form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'e'}));
    CHECK(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) == vulpes::ui::FormResult::saved);

    auto query = database.prepare("SELECT name FROM customer WHERE id = 1");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "Acme");
}

TEST_CASE("generated form cancels drafts and renders logical cells", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    auto dataset = form_dataset(database);
    vulpes::ui::RecordForm form{dataset, "Customer", vulpes::ui::FormMode::edit, "F8 Save"};
    vulpes::terminal::ScreenBuffer buffer{40, 10};

    form.render(buffer, {0, 0, 40, 10});
    CHECK(buffer.cell(2, 0).glyph == U'C');
    CHECK(buffer.cell(1, 1).glyph == U'i');
    CHECK(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::escape}) ==
          vulpes::ui::FormResult::cancelled);
    CHECK(dataset.mode() == vulpes::model::DatasetMode::browse);
}

TEST_CASE("generated form keeps blob columns read only", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE document(id INTEGER PRIMARY KEY, contents BLOB)");
    vulpes::model::Dataset dataset{database, vulpes::db::inspect_schema(database).front()};
    vulpes::ui::RecordForm form{dataset, "New document", vulpes::ui::FormMode::insert, "F8 Save"};

    REQUIRE(form.fields().size() == 2);
    CHECK(form.fields()[1].name == "contents");
    CHECK(form.fields()[1].kind == vulpes::ui::FormFieldKind::read_only);
}

TEST_CASE("generated form selects the failing constraint field and retains its draft", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    auto dataset = form_dataset(database);
    database.execute("CREATE UNIQUE INDEX customer_name ON customer(name);"
                     "INSERT INTO customer VALUES (2, 'Beta', 2.0, 1)");
    vulpes::ui::RecordForm form{dataset, "Customer", vulpes::ui::FormMode::edit, "F8 Save"};

    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    for (int count = 0; count < 4; ++count)
        static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::backspace}));
    for (const auto character : std::string_view{"Beta"})
        static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character,
                                                                 .character = static_cast<char32_t>(character)}));

    CHECK(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) == vulpes::ui::FormResult::redraw);
    REQUIRE(form.error_field_index());
    CHECK(*form.error_field_index() == 1);
    CHECK(form.selected_field_index() == 1);
    CHECK(form.fields().at(1).error.find("name") != std::string::npos);
    REQUIRE(dataset.draft_value("name"));
    CHECK(dataset.draft_value("name")->as_string() == "Beta");
}

TEST_CASE("generated form selects a foreign key by its inferred display field", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    database.execute(
        "CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
        "INSERT INTO customer VALUES (1, 'Acme'), (2, 'Beta');"
        "CREATE TABLE job(id INTEGER PRIMARY KEY, customer_id INTEGER REFERENCES customer(id), description TEXT);");
    const auto schemas = vulpes::db::inspect_schema(database);
    const auto job = std::ranges::find(schemas, "job", &vulpes::db::TableSchema::name);
    REQUIRE(job != schemas.end());
    vulpes::model::Dataset dataset{database, *job};
    vulpes::ui::RecordForm form{dataset, "New job", vulpes::ui::FormMode::insert, "F8 Save"};

    REQUIRE(form.fields().at(1).kind == vulpes::ui::FormFieldKind::lookup);
    CHECK(form.fields().at(1).lookup_options.at(0).label == "Acme");
    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::right}));
    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    static_cast<void>(
        form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'J'}));
    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}));

    auto query = database.prepare("SELECT customer_id, description FROM job");
    REQUIRE(query.step());
    CHECK(query.column(0).as_int() == 1);
    CHECK(query.column(1).as_string() == "J");
}
