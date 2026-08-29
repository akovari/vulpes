#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/report/export.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <pdfio.h>
#include <sstream>

namespace {

class TemporaryExport {
  public:
    explicit TemporaryExport(std::string_view extension)
        : path_{std::filesystem::temp_directory_path() /
                ("vulpes-export-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 std::string{extension})} {}

    ~TemporaryExport() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return path_; }

    [[nodiscard]] auto text() const -> std::string {
        std::ifstream input{path_, std::ios::binary};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

  private:
    std::filesystem::path path_;
};

auto sample_result(vulpes::db::Database& database) -> vulpes::db::SqlResult {
    return database.run_query(
        "SELECT 7 AS id, 12.5 AS amount, 'Žluťoučký, <fox>' AS label, NULL AS missing, x'00FF' AS payload");
}

} // namespace

TEST_CASE("CSV export preserves invariant SQLite values and RFC-style quoting", "[report][export][csv]") {
    vulpes::db::Database database{":memory:"};
    TemporaryExport file{".csv"};

    const auto summary = vulpes::report::export_result(
        sample_result(database), {.destination = file.path(), .format = vulpes::report::ExportFormat::csv});

    CHECK(summary.rows == 1);
    CHECK(summary.format == vulpes::report::ExportFormat::csv);
    const auto content = file.text();
    CHECK(content.starts_with("id,amount,label,missing,payload\r\n"));
    CHECK(content.find("12.5,\"Žluťoučký, <fox>\"") != std::string::npos);
    CHECK(content.find(",,X'00FF'\r\n") != std::string::npos);
}

TEST_CASE("JSON export preserves duplicate columns and typed row values", "[report][export][json]") {
    vulpes::db::Database database{":memory:"};
    TemporaryExport file{".json"};
    const auto result = database.run_query("SELECT 1 AS value, NULL AS value, x'AB' AS payload");

    static_cast<void>(vulpes::report::export_result(
        result, {.destination = file.path(), .format = vulpes::report::ExportFormat::json}));

    const auto document = nlohmann::json::parse(file.text());
    CHECK(document["columns"] == nlohmann::json::array({"value", "value", "payload"}));
    CHECK(document["rows"][0][0] == 1);
    CHECK(document["rows"][0][1].is_null());
    CHECK(document["rows"][0][2]["$blob"] == "AB");
}

TEST_CASE("human-readable exports apply locale display and HTML escaping", "[report][export][text][html]") {
    vulpes::db::Database database{":memory:"};
    const auto result = sample_result(database);
    TemporaryExport text_file{".txt"};
    TemporaryExport html_file{".html"};

    static_cast<void>(vulpes::report::export_result(
        result, {.destination = text_file.path(), .format = vulpes::report::ExportFormat::text, .locale = "cs-CZ"}));
    static_cast<void>(vulpes::report::export_result(result, {.destination = html_file.path(),
                                                             .format = vulpes::report::ExportFormat::html,
                                                             .title = "Liščí <report>",
                                                             .locale = "cs-CZ"}));

    CHECK(text_file.text().find("12,5") != std::string::npos);
    CHECK(html_file.text().find("<meta charset=\"utf-8\">") != std::string::npos);
    CHECK(html_file.text().find("Liščí &lt;report&gt;") != std::string::npos);
    CHECK(html_file.text().find("Žluťoučký, &lt;fox&gt;") != std::string::npos);
}

TEST_CASE("PDF export writes a Unicode-capable document", "[report][export][pdf]") {
    vulpes::db::Database database{":memory:"};
    TemporaryExport file{".pdf"};

    const auto summary = vulpes::report::export_result(
        sample_result(database),
        {.destination = file.path(), .format = vulpes::report::ExportFormat::pdf, .title = "Liščí report"});

    CHECK(summary.format == vulpes::report::ExportFormat::pdf);
    const auto content = file.text();
    CHECK(content.starts_with("%PDF-"));
    CHECK(content.size() > 50'000);
    CHECK(content.find("%%EOF") != std::string::npos);

    const auto native_path = file.path().string();
    auto* document = pdfioFileOpen(
        native_path.c_str(), nullptr, nullptr, [](pdfio_file_t*, const char*, void*) { return true; }, nullptr);
    REQUIRE(document != nullptr);
    CHECK(pdfioFileGetNumPages(document) == 1);
    CHECK(pdfioFileClose(document));
}

TEST_CASE("export refuses truncation invalid text and implicit overwrite", "[report][export][safety]") {
    vulpes::db::Database database{":memory:"};
    TemporaryExport file{".csv"};
    {
        std::ofstream existing{file.path(), std::ios::binary};
        existing << "keep";
    }

    CHECK_THROWS_AS(
        vulpes::report::export_result(sample_result(database),
                                      {.destination = file.path(), .format = vulpes::report::ExportFormat::csv}),
        vulpes::Error);
    CHECK(file.text() == "keep");

    auto truncated = database.run_query("SELECT 1 AS value UNION ALL SELECT 2", 1);
    TemporaryExport truncated_file{".csv"};
    CHECK_THROWS_AS(vulpes::report::export_result(
                        truncated, {.destination = truncated_file.path(), .format = vulpes::report::ExportFormat::csv}),
                    vulpes::Error);

    auto invalid = database.run_query("SELECT CAST(x'FF' AS TEXT) AS value");
    TemporaryExport invalid_file{".json"};
    CHECK_THROWS_AS(vulpes::report::export_result(
                        invalid, {.destination = invalid_file.path(), .format = vulpes::report::ExportFormat::json}),
                    vulpes::Error);
    CHECK_FALSE(std::filesystem::exists(invalid_file.path()));
}

TEST_CASE("export replaces a destination only when explicitly requested", "[report][export][overwrite]") {
    vulpes::db::Database database{":memory:"};
    TemporaryExport file{".csv"};
    {
        std::ofstream existing{file.path(), std::ios::binary};
        existing << "old";
    }

    const auto summary = vulpes::report::export_query(
        database, "SELECT 'new' AS value", 10,
        {.destination = file.path(), .format = vulpes::report::ExportFormat::csv, .overwrite = true});

    CHECK(summary.destination == file.path());
    CHECK(file.text() == "value\r\nnew\r\n");
}

TEST_CASE("export formats parse explicitly or from the destination extension", "[report][export][format]") {
    CHECK(vulpes::report::parse_export_format("CSV") == vulpes::report::ExportFormat::csv);
    CHECK(vulpes::report::parse_export_format("txt") == vulpes::report::ExportFormat::text);
    CHECK(vulpes::report::infer_export_format("result.HTML") == vulpes::report::ExportFormat::html);
    CHECK_THROWS_AS(vulpes::report::infer_export_format("result"), vulpes::Error);
    CHECK_THROWS_AS(vulpes::report::parse_export_format("xml"), vulpes::Error);
}
