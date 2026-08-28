#include "vulpes/appmeta/metadata.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("application metadata validates schema enhancing field policies", "[appmeta]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT);"
                     "CREATE TABLE invoice(id INTEGER PRIMARY KEY, customer_id INTEGER REFERENCES customer(id), "
                     "issued_at TEXT, total REAL)");
    const auto schema = vulpes::db::inspect_schema(database);
    vulpes::appmeta::ApplicationMetadata metadata{{{
        .name = "invoice",
        .label = "Invoice",
        .fields = {{.name = "customer_id",
                    .label = "Customer",
                    .order = 1,
                    .lookup = vulpes::appmeta::LookupMetadata{.display_field = "name", .search_fields = {"name"}}},
                   {.name = "issued_at",
                    .label = "Issued",
                    .order = 2,
                    .format = vulpes::appmeta::FieldFormat::date_time,
                    .time_zone = "Europe/Prague"},
                   {.name = "total", .format = vulpes::appmeta::FieldFormat::currency, .currency_code = "EUR"}},
    }}};

    CHECK_NOTHROW(metadata.validate(schema));
    REQUIRE(metadata.table("invoice") != nullptr);
    CHECK(metadata.table("invoice")->field("customer_id")->label == "Customer");
}

TEST_CASE("application metadata rejects unsafe or unresolved overrides", "[appmeta][validation]") {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE item(id INTEGER PRIMARY KEY, amount REAL)");
    const auto schema = vulpes::db::inspect_schema(database);

    vulpes::appmeta::ApplicationMetadata unknown{{{.name = "missing"}}};
    CHECK_THROWS_AS(unknown.validate(schema), vulpes::Error);

    vulpes::appmeta::ApplicationMetadata invalid_currency{{{
        .name = "item",
        .fields = {{.name = "amount", .format = vulpes::appmeta::FieldFormat::currency}},
    }}};
    CHECK_THROWS_AS(invalid_currency.validate(schema), vulpes::Error);

    vulpes::appmeta::ApplicationMetadata invalid_lookup{{{
        .name = "item",
        .fields = {{.name = "amount", .lookup = vulpes::appmeta::LookupMetadata{}}},
    }}};
    CHECK_THROWS_AS(invalid_lookup.validate(schema), vulpes::Error);

    vulpes::appmeta::ApplicationMetadata invalid_temporal{{{
        .name = "item",
        .fields = {{.name = "amount", .format = vulpes::appmeta::FieldFormat::date_time, .time_zone = "Not/AZone"}},
    }}};
    CHECK_THROWS_AS(invalid_temporal.validate(schema), vulpes::Error);
}
