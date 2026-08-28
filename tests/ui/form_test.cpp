#include "vulpes/appmeta/metadata.hpp"
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
    CHECK(form.selected_field_index() == 1);

    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::backspace}));
    static_cast<void>(
        form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'e'}));
    CHECK(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) == vulpes::ui::FormResult::saved);

    auto query = database.prepare("SELECT name FROM customer WHERE id = 1");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "Acme");
}

TEST_CASE("generated form edits text at a UTF-8 cursor", "[ui][form][editor]") {
    vulpes::db::Database database{":memory:"};
    auto dataset = form_dataset(database);
    vulpes::ui::RecordForm form{dataset, "Customer", vulpes::ui::FormMode::edit, "F8 Save"};

    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::home}));
    static_cast<void>(
        form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'Ž'}));
    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::right}));
    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::delete_key}));
    CHECK(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) == vulpes::ui::FormResult::saved);

    auto query = database.prepare("SELECT name FROM customer WHERE id = 1");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "ŽAme");
}

TEST_CASE("generated form cancels drafts and renders logical cells", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    auto dataset = form_dataset(database);
    vulpes::ui::RecordForm form{dataset, "Customer", vulpes::ui::FormMode::edit, "F8 Save"};
    vulpes::terminal::ScreenBuffer buffer{40, 10};

    form.render(buffer, {0, 0, 40, 10});
    CHECK(buffer.cell(0, 0).glyph == U'┌');
    CHECK(buffer.cell(3, 0).glyph == U'C');
    CHECK(buffer.cell(1, 1).glyph == U'i');
    CHECK(buffer.cell(19, 2).style ==
          vulpes::ui::theme(vulpes::ui::ThemeName::midnight).style(vulpes::ui::ThemeRole::input_focus));
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

TEST_CASE("generated form keeps the focused field visible in a compact window", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    database.execute(
        "CREATE TABLE many_fields(id INTEGER PRIMARY KEY, a TEXT, b TEXT, c TEXT, d TEXT, e TEXT, f TEXT, g TEXT)");
    vulpes::model::Dataset dataset{database, vulpes::db::inspect_schema(database).front()};
    vulpes::ui::RecordForm form{dataset, "Compact form", vulpes::ui::FormMode::insert, "F8 Save"};
    vulpes::terminal::ScreenBuffer buffer{40, 6};

    for (int index = 0; index < 6; ++index)
        static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    form.render(buffer, {0, 0, 40, 6});

    CHECK(form.selected_field_index() == 6);
    CHECK(buffer.cell(39, 1).glyph == U'▲');
    CHECK(buffer.cell(39, 3).glyph == U'▼');
    CHECK(buffer.cell(1, 2).glyph == U'f');
}

TEST_CASE("generated form infers boolean controls without treating ordinary integers as booleans", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE member(id INTEGER PRIMARY KEY, subscribed BOOLEAN, quantity INTEGER)");
    vulpes::model::Dataset dataset{database, vulpes::db::inspect_schema(database).front()};
    vulpes::ui::RecordForm form{dataset, "New member", vulpes::ui::FormMode::insert, "F8 Save"};

    REQUIRE(form.fields().size() == 3);
    CHECK(form.fields().at(1).kind == vulpes::ui::FormFieldKind::checkbox);
    CHECK(form.fields().at(2).kind == vulpes::ui::FormFieldKind::number);
}

TEST_CASE("generated form selects the failing constraint field and retains its draft", "[ui][form]") {
    vulpes::db::Database database{":memory:"};
    auto dataset = form_dataset(database);
    database.execute("CREATE UNIQUE INDEX customer_name ON customer(name);"
                     "INSERT INTO customer VALUES (2, 'Beta', 2.0, 1)");
    vulpes::ui::RecordForm form{dataset, "Customer", vulpes::ui::FormMode::edit, "F8 Save"};

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

TEST_CASE("generated form applies metadata labels order visibility and read-only policy", "[ui][form][appmeta]") {
    vulpes::db::Database database{":memory:"};
    auto dataset = form_dataset(database);
    const vulpes::appmeta::TableMetadata metadata{
        .name = "customer",
        .fields = {{.name = "id", .visible = false},
                   {.name = "name", .label = "Customer name", .order = 1},
                   {.name = "balance", .visible = false},
                   {.name = "active", .label = "Enabled", .order = 0, .read_only = true}},
    };
    vulpes::ui::RecordForm form{
        dataset, "Customer", vulpes::ui::FormMode::edit, "F8 Save", vulpes::ui::theme(vulpes::ui::ThemeName::midnight),
        nullptr, &metadata};

    REQUIRE(form.fields().size() == 2);
    CHECK(form.fields()[0].name == "active");
    CHECK(form.fields()[0].label == "Enabled");
    CHECK(form.fields()[0].read_only);
    CHECK(form.fields()[1].name == "name");
    CHECK(form.fields()[1].label == "Customer name");
    CHECK(form.selected_field_index() == 1);
}

TEST_CASE("generated form requests a configured searchable relationship lookup", "[ui][form][lookup][appmeta]") {
    vulpes::db::Database database{":memory:"};
    database.execute(
        "CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL, code TEXT);"
        "INSERT INTO customer VALUES (1, 'Acme', 'A'), (2, 'Beta', 'B');"
        "CREATE TABLE job(id INTEGER PRIMARY KEY, customer_id INTEGER REFERENCES customer(id), description TEXT);");
    const auto schemas = vulpes::db::inspect_schema(database);
    const auto job = std::ranges::find(schemas, "job", &vulpes::db::TableSchema::name);
    REQUIRE(job != schemas.end());
    vulpes::model::Dataset dataset{database, *job};
    const vulpes::appmeta::TableMetadata metadata{
        .name = "job",
        .fields = {{.name = "customer_id",
                    .label = "Customer",
                    .lookup = vulpes::appmeta::LookupMetadata{.display_field = "name",
                                                              .search_fields = {"name", "code"},
                                                              .result_limit = 25}}},
    };
    vulpes::ui::RecordForm form{
        dataset, "New job", vulpes::ui::FormMode::insert, "F8 Save", vulpes::ui::theme(vulpes::ui::ThemeName::midnight),
        nullptr, &metadata};

    static_cast<void>(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::down}));
    CHECK(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::enter}) ==
          vulpes::ui::FormResult::lookup_requested);
    const auto request = form.lookup_request();
    REQUIRE(request);
    CHECK(request->field == "customer_id");
    CHECK(request->query.search_fields == std::vector<std::string>{"name", "code"});
    CHECK(request->query.limit == 25);

    form.select_lookup("customer_id", {.value = 2, .label = "Beta"});
    CHECK(form.fields()[1].editor.text() == "Beta");
    CHECK(form.is_dirty());
}

TEST_CASE("generated temporal controls normalize explicitly annotated text fields", "[ui][form][appmeta][temporal]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE event(id INTEGER PRIMARY KEY, occurred_at TEXT)");
    const auto schema = vulpes::db::inspect_schema(database);
    vulpes::model::Dataset dataset{database, schema.front()};
    const vulpes::appmeta::TableMetadata metadata{
        .name = "event",
        .fields = {{.name = "id", .visible = false},
                   {.name = "occurred_at",
                    .label = "Occurred",
                    .format = vulpes::appmeta::FieldFormat::date_time,
                    .time_zone = "Europe/Prague"}},
    };
    vulpes::ui::RecordForm form{dataset,
                                "New event",
                                vulpes::ui::FormMode::insert,
                                "F8 Save",
                                vulpes::ui::theme(vulpes::ui::ThemeName::midnight),
                                nullptr,
                                &metadata};

    REQUIRE(form.fields().size() == 1);
    CHECK(form.fields().front().kind == vulpes::ui::FormFieldKind::date_time);
    for (const auto character : std::u32string{U"2024-01-02T16:04:05+01:00"})
        static_cast<void>(
            form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = character}));
    CHECK(form.handle(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) == vulpes::ui::FormResult::saved);

    auto query = database.prepare("SELECT occurred_at FROM event");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "2024-01-02T15:04:05Z");
}
