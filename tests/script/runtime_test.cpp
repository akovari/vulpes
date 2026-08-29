#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/model/dataset.hpp"
#include "vulpes/script/runtime.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string_view>

namespace {

auto customer_schema(vulpes::db::Database& database) -> vulpes::db::TableSchema {
    const auto schema = vulpes::db::inspect_schema(database);
    return schema.front();
}

auto make_database() -> vulpes::db::Database {
    vulpes::db::Database database{":memory:"};
    database.execute("CREATE TABLE customer(id INTEGER PRIMARY KEY, name TEXT NOT NULL, balance REAL, note BLOB);");
    return database;
}

} // namespace

TEST_CASE("Lua lifecycle scripts transform drafts and execute in the dataset transaction", "[script][dataset]") {
    auto database = make_database();
    vulpes::script::Runtime scripts{{
        {.name = "normalize-customer",
         .hook = vulpes::script::Hook::before_insert,
         .table = "customer",
         .source = "record.name = string.upper(record.name)\nrecord.balance = record.balance or 0"},
        {.name = "stamp-update",
         .hook = vulpes::script::Hook::before_update,
         .table = "customer",
         .source = "record.name = record.name .. '!'"},
        {.name = "verify-update",
         .hook = vulpes::script::Hook::after_update,
         .table = "customer",
         .source = "assert(record.name == 'ADA!')"},
    }};
    vulpes::model::Dataset dataset{database, customer_schema(database), 100, &scripts};

    dataset.begin_insert();
    dataset.set("name", "Ada");
    dataset.save();

    auto query = database.prepare("SELECT name, balance FROM customer");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "ADA");
    CHECK(query.column(1).as_double() == 0.0);

    dataset.begin_edit();
    dataset.set("balance", 10.0);
    dataset.save();
    query = database.prepare("SELECT name, balance FROM customer");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "ADA!");
    CHECK(query.column(1).as_double() == 10.0);
}

TEST_CASE("Lua failures roll back dataset writes and preserve an editable draft", "[script][dataset]") {
    auto database = make_database();
    database.execute("INSERT INTO customer(name, balance) VALUES('Ada', 1.0)");
    vulpes::script::Runtime scripts{{
        {.name = "reject-update",
         .hook = vulpes::script::Hook::after_update,
         .table = "customer",
         .source = "error('balance review failed')"},
        {.name = "protect-delete",
         .hook = vulpes::script::Hook::before_delete,
         .table = "customer",
         .source = "if record.name == 'Ada' then error('protected customer') end"},
    }};
    vulpes::model::Dataset dataset{database, customer_schema(database), 100, &scripts};

    dataset.begin_edit();
    dataset.set("balance", 2.0);
    try {
        dataset.save();
        FAIL("the after_update script should reject the write");
    } catch (const vulpes::Error& error) {
        CHECK(error.category() == vulpes::ErrorCategory::script);
        CHECK(std::string_view{error.what()}.contains("balance review failed"));
    }
    CHECK(dataset.mode() == vulpes::model::DatasetMode::edit);
    REQUIRE(dataset.draft_value("balance"));
    CHECK(dataset.draft_value("balance")->as_double() == 2.0);
    auto query = database.prepare("SELECT balance FROM customer");
    REQUIRE(query.step());
    CHECK(query.column(0).as_double() == 1.0);

    dataset.cancel();
    try {
        dataset.erase();
        FAIL("the before_delete script should reject deletion");
    } catch (const vulpes::Error& error) {
        CHECK(error.category() == vulpes::ErrorCategory::script);
        CHECK(std::string_view{error.what()}.contains("protected customer"));
    }
    query = database.prepare("SELECT count(*) FROM customer");
    REQUIRE(query.step());
    CHECK(query.column(0).as_int() == 1);
}

TEST_CASE("Lua exposes only the restricted data host and enforces execution limits", "[script][sandbox]") {
    vulpes::script::Runtime safe{{
        {.name = "restricted-libraries",
         .hook = vulpes::script::Hook::on_open,
         .source = "assert(io == nil and os == nil and package == nil and debug == nil)\n"
                   "assert(dofile == nil and loadfile == nil and load == nil and print == nil)\n"
                   "assert(context.hook == 'on_open')"},
        {.name = "command-context",
         .hook = vulpes::script::Hook::on_command,
         .command = "browse",
         .source = "assert(context.command == 'browse')"},
    }};
    CHECK_NOTHROW(safe.on_open());
    CHECK_NOTHROW(safe.on_command("browse"));
    CHECK_NOTHROW(safe.on_command("tables"));

    vulpes::script::Runtime limited{
        {{.name = "loop", .hook = vulpes::script::Hook::on_open, .source = "while true do end"}},
        {.memory_bytes = 256U * 1024U, .instruction_count = 5'000U}};
    try {
        limited.on_open();
        FAIL("the Lua instruction limit should stop an infinite loop");
    } catch (const vulpes::Error& error) {
        CHECK(error.category() == vulpes::ErrorCategory::script);
        CHECK(std::string_view{error.what()}.contains("instruction budget"));
    }
}

TEST_CASE("Lua rejects read-only BLOB and unknown record fields", "[script][sandbox]") {
    auto database = make_database();
    vulpes::script::Runtime scripts{{
        {.name = "bad-record-write",
         .hook = vulpes::script::Hook::before_insert,
         .table = "customer",
         .source = "record.unknown = 'nope'"},
    }};
    vulpes::model::Dataset dataset{database, customer_schema(database), 100, &scripts};
    dataset.begin_insert();
    dataset.set("name", "Ada");
    CHECK_THROWS_AS(dataset.save(), vulpes::Error);
    CHECK(dataset.mode() == vulpes::model::DatasetMode::insert);

    vulpes::script::Runtime blob_scripts{{
        {.name = "blob-write",
         .hook = vulpes::script::Hook::before_insert,
         .table = "customer",
         .source = "record.note = 'nope'"},
    }};
    vulpes::model::Dataset blob_dataset{database, customer_schema(database), 100, &blob_scripts};
    blob_dataset.begin_insert();
    blob_dataset.set("name", "Grace");
    try {
        blob_dataset.save();
        FAIL("Lua must not gain a BLOB field bridge");
    } catch (const vulpes::Error& error) {
        CHECK(error.category() == vulpes::ErrorCategory::script);
        CHECK(std::string_view{error.what()}.contains("BLOB"));
    }
}
