#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes;

namespace {

auto make_dataset_database() -> db::Database {
    db::Database database{":memory:"};
    database.execute("CREATE TABLE customer("
                     "id INTEGER PRIMARY KEY, name TEXT NOT NULL, city TEXT, balance REAL, active INTEGER);"
                     "INSERT INTO customer(name, city, balance, active) VALUES"
                     "('Ábel', 'Prague', 12.5, 1),"
                     "('Beta', 'Brno', 4.0, 1),"
                     "('Cora', NULL, 8.0, 0),"
                     "('Delta', 'Prague', 1.0, 1),"
                     "('Echo', 'Ostrava', 6.0, 0);");
    return database;
}

auto customer_schema(db::Database& database) -> db::TableSchema {
    const auto schema = db::inspect_schema(database);
    return schema.front();
}

} // namespace

TEST_CASE("dataset pages and navigates deterministic primary-key rows", "[model][dataset]") {
    auto database = make_dataset_database();
    model::Dataset dataset{database, customer_schema(database), 2};

    REQUIRE(dataset.total_count() == 5);
    REQUIRE(dataset.rows().size() == 2);
    REQUIRE(dataset.current());
    CHECK(dataset.current()->at("name").as_string() == "Ábel");

    CHECK(dataset.next());
    CHECK(dataset.current()->at("name").as_string() == "Beta");
    CHECK(dataset.next());
    CHECK(dataset.page_offset() == 2);
    CHECK(dataset.current()->at("name").as_string() == "Cora");
    CHECK(dataset.previous());
    CHECK(dataset.page_offset() == 0);
    CHECK(dataset.current()->at("name").as_string() == "Beta");

    CHECK(dataset.last());
    CHECK(dataset.page_offset() == 4);
    CHECK(dataset.current()->at("name").as_string() == "Echo");
    CHECK_FALSE(dataset.next());
    REQUIRE(dataset.current_identity());
    CHECK(dataset.current_identity()->fields == std::vector<std::string>{"id"});
    CHECK(dataset.current_identity()->values.front().as_int() == 5);
    CHECK(dataset.is_editable());
}

TEST_CASE("dataset uses keyset boundaries for a stable single-column primary key", "[model][dataset][paging]") {
    auto database = make_dataset_database();
    model::Dataset dataset{database, customer_schema(database), 2};

    REQUIRE(dataset.next());
    CHECK(dataset.current()->at("id").as_int() == 2);
    database.execute("INSERT INTO customer(id, name) VALUES (0, 'Inserted before current page')");

    // An OFFSET page would repeat id 2 after the insertion. The keyset cursor
    // continues strictly after its last visible primary-key value.
    REQUIRE(dataset.next());
    CHECK(dataset.page_offset() == 2);
    CHECK(dataset.current()->at("id").as_int() == 3);
    REQUIRE(dataset.previous());
    CHECK(dataset.page_offset() == 0);
    CHECK(dataset.current()->at("id").as_int() == 2);
}

TEST_CASE("dataset binds filters, orders, and escapes search text", "[model][dataset]") {
    auto database = make_dataset_database();
    model::Dataset dataset{database, customer_schema(database), 10};

    dataset.where({"active", model::FilterOperator::equal, true})
        .where({"balance", model::FilterOperator::greater, 3.0})
        .order_by("name", model::SortDirection::descending);
    REQUIRE(dataset.rows().size() == 2);
    CHECK(dataset.rows().front().at("name").as_string() == "Ábel");
    CHECK(dataset.rows().back().at("name").as_string() == "Beta");

    dataset.clear_filters().search("Prague");
    REQUIRE(dataset.rows().size() == 2);
    CHECK(dataset.rows().front().at("name").as_string() == "Ábel");
    CHECK(dataset.rows().back().at("name").as_string() == "Delta");

    dataset.search("%");
    CHECK(dataset.rows().empty());
}

TEST_CASE("dataset uses correct NULL predicate semantics", "[model][dataset]") {
    auto database = make_dataset_database();
    model::Dataset dataset{database, customer_schema(database)};

    dataset.where({"city", model::FilterOperator::equal, nullptr});
    REQUIRE(dataset.rows().size() == 1);
    CHECK(dataset.rows().front().at("name").as_string() == "Cora");

    dataset.clear_filters().where({"city", model::FilterOperator::not_equal, nullptr});
    CHECK(dataset.total_count() == 4);
}

TEST_CASE("dataset exposes bounded foreign-key lookup options with display-field heuristics", "[model][dataset]") {
    auto database = make_dataset_database();
    database.execute("CREATE TABLE region(id INTEGER PRIMARY KEY, code TEXT NOT NULL);"
                     "INSERT INTO region VALUES (1, 'CZ'), (2, 'AT');"
                     "CREATE TABLE office(id INTEGER PRIMARY KEY, region_id INTEGER REFERENCES region(id));");
    const auto schemas = db::inspect_schema(database);
    const auto office = std::ranges::find(schemas, "office", &db::TableSchema::name);
    REQUIRE(office != schemas.end());
    model::Dataset dataset{database, *office};

    const auto options = dataset.lookup_options("region_id", 1);
    REQUIRE(options.size() == 1);
    CHECK(options.front().value.as_int() == 2);
    CHECK(options.front().label == "AT");
    CHECK_THROWS_AS(dataset.lookup_options("id"), Error);
    CHECK_THROWS_AS(dataset.lookup_options("region_id", 0), Error);
}

TEST_CASE("dataset rejects unknown fields and invalid NULL comparisons", "[model][dataset]") {
    auto database = make_dataset_database();
    model::Dataset dataset{database, customer_schema(database)};

    CHECK_THROWS_AS(dataset.order_by("not_a_field"), Error);
    CHECK_THROWS_AS(dataset.where({"balance", model::FilterOperator::greater, nullptr}), Error);
}

TEST_CASE("views remain readable but advertise no edit capability", "[model][dataset]") {
    auto database = make_dataset_database();
    database.execute("CREATE VIEW active_customer AS SELECT * FROM customer WHERE active = 1");
    const auto schema = db::inspect_schema(database);
    const auto view = std::ranges::find(schema, "active_customer", &db::TableSchema::name);
    REQUIRE(view != schema.end());

    model::Dataset dataset{database, *view};
    CHECK(dataset.rows().size() == 3);
    CHECK_FALSE(dataset.is_editable());
    CHECK_FALSE(dataset.current_identity());
    CHECK_THROWS_AS(dataset.begin_insert(), Error);
    CHECK_THROWS_AS(dataset.begin_edit(), Error);
    CHECK_THROWS_AS(dataset.erase(), Error);
}

TEST_CASE("dataset inserts records and preserves database defaults", "[model][dataset][edit]") {
    auto database = make_dataset_database();
    database.execute("ALTER TABLE customer ADD COLUMN status TEXT NOT NULL DEFAULT 'new'");
    model::Dataset dataset{database, customer_schema(database)};

    dataset.begin_insert();
    CHECK(dataset.mode() == model::DatasetMode::insert);
    CHECK_FALSE(dataset.is_dirty());
    CHECK_FALSE(dataset.draft_value("name"));

    dataset.set("name", "New customer").set("city", "Plzen").set("balance", 7.5).set("active", true);
    CHECK(dataset.is_dirty());
    REQUIRE(dataset.draft_value("name"));
    CHECK(dataset.draft_value("name")->as_string() == "New customer");
    dataset.save();

    CHECK(dataset.mode() == model::DatasetMode::browse);
    CHECK_FALSE(dataset.is_dirty());
    auto query = database.prepare("SELECT name, city, balance, active, status FROM customer WHERE name = ?");
    query.bind(1, "New customer");
    REQUIRE(query.step());
    CHECK(query.column(1).as_string() == "Plzen");
    CHECK(query.column(2).as_double() == 7.5);
    CHECK(query.column(3).as_int() == 1);
    CHECK(query.column(4).as_string() == "new");
}

TEST_CASE("dataset updates, cancels, and deletes through stable row identity", "[model][dataset][edit]") {
    auto database = make_dataset_database();
    model::Dataset dataset{database, customer_schema(database)};

    dataset.begin_edit();
    REQUIRE(dataset.draft_value("name"));
    REQUIRE(dataset.current());
    CHECK(*dataset.draft_value("name") == dataset.current()->at("name"));
    dataset.set("city", "Olomouc");
    CHECK(dataset.is_dirty());
    dataset.cancel();
    CHECK(dataset.mode() == model::DatasetMode::browse);
    CHECK_FALSE(dataset.is_dirty());

    dataset.begin_edit();
    dataset.set("city", "Olomouc");
    CHECK_THROWS_AS(dataset.set("id", 88), Error);
    CHECK_THROWS_AS(dataset.set("name", nullptr), Error);
    dataset.save();

    auto query = database.prepare("SELECT city FROM customer WHERE id = 1");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "Olomouc");

    static_cast<void>(dataset.first());
    dataset.erase();
    CHECK(dataset.total_count() == 4);
    auto count = database.prepare("SELECT count(*) FROM customer WHERE id = 1");
    REQUIRE(count.step());
    CHECK(count.column(0).as_int() == 0);
}

TEST_CASE("dataset retains edit state after a failed constraint save", "[model][dataset][edit]") {
    auto database = make_dataset_database();
    database.execute("CREATE UNIQUE INDEX customer_name ON customer(name)");
    model::Dataset dataset{database, customer_schema(database)};

    dataset.begin_edit();
    dataset.set("name", "Beta");
    CHECK_THROWS_AS(dataset.save(), Error);
    CHECK(dataset.mode() == model::DatasetMode::edit);
    CHECK(dataset.is_dirty());
    REQUIRE(dataset.draft_value("name"));
    CHECK(dataset.draft_value("name")->as_string() == "Beta");

    dataset.cancel();
    auto query = database.prepare("SELECT count(*) FROM customer WHERE id = 1 AND name = 'Beta'");
    REQUIRE(query.step());
    CHECK(query.column(0).as_int() == 0);
}

TEST_CASE("tables without primary keys remain browse-only", "[model][dataset][edit]") {
    db::Database database{":memory:"};
    database.execute("CREATE TABLE note(body TEXT); INSERT INTO note VALUES ('read me')");
    const auto schema = db::inspect_schema(database);
    model::Dataset dataset{database, schema.front()};

    CHECK_FALSE(dataset.is_editable());
    CHECK_THROWS_AS(dataset.begin_insert(), Error);
    CHECK_THROWS_AS(dataset.begin_edit(), Error);
    CHECK_THROWS_AS(dataset.erase(), Error);
}
