#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/transaction.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>

using namespace vulpes::db;

namespace {

class TemporaryDatabaseFile {
  public:
    TemporaryDatabaseFile()
        : path_{std::filesystem::temp_directory_path() /
                ("vulpes-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 ".sqlite")} {}
    ~TemporaryDatabaseFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("values round-trip through SQLite", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY, name TEXT, score REAL, payload BLOB)");

    auto insert = database.prepare("INSERT INTO sample(name, score) VALUES (?, ?)");
    insert.bind(1, "Liška").bind(2, 4.5).execute();

    auto query = database.prepare("SELECT id, name, score, payload FROM sample");
    REQUIRE(query.step());
    CHECK(query.column(0).as_int() == 1);
    CHECK(query.column(1).as_string() == "Liška");
    CHECK(query.column(2).as_double() == 4.5);
    CHECK(query.column(3).is_null());
    CHECK_FALSE(query.step());
}

TEST_CASE("an uncommitted transaction rolls back", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(value INTEGER)");
    {
        Transaction transaction{database};
        database.execute("INSERT INTO sample VALUES (42)");
    }
    auto count = database.prepare("SELECT count(*) FROM sample");
    REQUIRE(count.step());
    CHECK(count.column(0).as_int() == 0);
}

TEST_CASE("execute accepts a complete SQL script", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE first(value TEXT);"
                     "CREATE TABLE second(value TEXT);"
                     "INSERT INTO second VALUES ('ready');");

    auto query = database.prepare("SELECT value FROM second");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string() == "ready");
}

TEST_CASE("run_sql owns the final tabular result and bounds rows", "[db]") {
    Database database{":memory:"};
    const auto result = database.run_sql("CREATE TABLE sample(id INTEGER);"
                                         "INSERT INTO sample VALUES (1), (2), (3);"
                                         "SELECT id FROM sample ORDER BY id;",
                                         2);

    CHECK(result.changes == 3);
    CHECK(result.columns == std::vector<std::string>{"id"});
    REQUIRE(result.rows.size() == 2);
    CHECK(result.rows.front().at("id").as_int() == 1);
    CHECK(result.rows.back().at("id").as_int() == 2);
    CHECK(result.truncated);
    CHECK_THROWS_AS(database.run_sql("SELECT 1", 0), vulpes::Error);
}

TEST_CASE("rows own values independently from their statement", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(value TEXT); INSERT INTO sample VALUES ('persist');");
    auto query = database.prepare("SELECT value FROM sample");
    REQUIRE(query.step());
    const auto row = query.row();
    query.reset();
    CHECK(row.size() == 1);
    CHECK(row.column_name(0) == "value");
    CHECK(row.at("value").as_string() == "persist");
    CHECK_THROWS_AS(row.at("missing"), std::out_of_range);
}

TEST_CASE("nested transactions fail before SQLite execution", "[db]") {
    Database database{":memory:"};
    Transaction outer{database};
    CHECK(database.in_transaction());
    CHECK_THROWS_AS(Transaction{database}, vulpes::Error);
    outer.rollback();
    CHECK_FALSE(database.in_transaction());
}

TEST_CASE("constraint failures retain SQLite result codes", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(value TEXT UNIQUE); INSERT INTO sample VALUES ('one');");
    try {
        database.execute("INSERT INTO sample VALUES ('one')");
        FAIL("expected a unique constraint violation");
    } catch (const vulpes::Error& error) {
        CHECK(error.category() == vulpes::ErrorCategory::constraint);
        CHECK(error.native_code() != 0);
    }
}

TEST_CASE("values preserve blobs, empty values, and SQLite text bytes", "[db]") {
    Database database{":memory:"};
    database.execute("CREATE TABLE sample(empty_text TEXT, empty_blob BLOB, payload BLOB, raw_text TEXT)");
    const Blob payload{std::byte{0x00}, std::byte{0x7F}, std::byte{0xFF}};
    const std::string invalid_utf8 = std::string{"valid"} + static_cast<char>(0xFF) + "bytes";

    auto insert = database.prepare("INSERT INTO sample VALUES (:text, :empty_blob, :payload, :raw_text)");
    insert.bind(":text", "")
        .bind(":empty_blob", Blob{})
        .bind(":payload", payload)
        .bind(":raw_text", invalid_utf8)
        .execute();

    auto query = database.prepare("SELECT empty_text, empty_blob, payload, raw_text FROM sample");
    REQUIRE(query.step());
    CHECK(query.column(0).as_string().empty());
    CHECK(query.column(1).as_blob().empty());
    CHECK(std::ranges::equal(query.column(2).as_blob(), payload));
    CHECK(query.column(3).as_string() == invalid_utf8);
}

TEST_CASE("database is movable and read-only mode rejects writes", "[db]") {
    TemporaryDatabaseFile file;
    {
        Database writer{file.path()};
        writer.execute("CREATE TABLE sample(value INTEGER); INSERT INTO sample VALUES (7)");
        Database moved{std::move(writer)};
        auto query = moved.prepare("SELECT value FROM sample");
        REQUIRE(query.step());
        CHECK(query.column(0).as_int() == 7);
    }
    {
        Database read_only{file.path(), OpenMode::read_only};
        auto query = read_only.prepare("SELECT value FROM sample");
        REQUIRE(query.step());
        CHECK(query.column(0).as_int() == 7);
        CHECK_THROWS_AS(read_only.execute("INSERT INTO sample VALUES (8)"), vulpes::Error);
    }
}

TEST_CASE("a locked database reports a busy error after its configured timeout", "[db]") {
    TemporaryDatabaseFile file;
    Database first{file.path()};
    first.execute("CREATE TABLE sample(value INTEGER)");
    Database second{file.path()};
    second.execute("PRAGMA busy_timeout = 1");

    Transaction transaction{first};
    first.execute("INSERT INTO sample VALUES (1)");
    try {
        second.execute("INSERT INTO sample VALUES (2)");
        FAIL("expected a busy error");
    } catch (const vulpes::Error& error) {
        CHECK(error.category() == vulpes::ErrorCategory::database);
        CHECK(error.native_code() != 0);
    }
    transaction.rollback();
}
