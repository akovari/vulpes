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
}
