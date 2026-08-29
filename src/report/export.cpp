#include "vulpes/report/export.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/core/formatting.hpp"

#include "pdf_export.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <system_error>
#include <utf8proc.h>
#include <variant>

namespace vulpes::report {
namespace {

auto lowercase_ascii(std::string text) -> std::string {
    std::ranges::transform(text, text.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a')
                                                    : static_cast<char>(character);
    });
    return text;
}

auto path_text(const std::filesystem::path& path) -> std::string {
    const auto encoded = path.generic_u8string();
    return {encoded.begin(), encoded.end()};
}

auto sibling_artifact_path(const std::filesystem::path& parent, const std::filesystem::path& destination,
                           std::string_view kind, int suffix) -> std::filesystem::path {
    auto name = std::filesystem::path{"."};
    name += destination.filename();
    name += kind;
    name += std::to_string(suffix);
    return parent / name;
}

void validate_utf8(std::string_view text, std::string_view description) {
    const auto* cursor = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    auto remaining = static_cast<utf8proc_ssize_t>(text.size());
    while (remaining > 0) {
        utf8proc_int32_t code_point{};
        const auto consumed = utf8proc_iterate(cursor, remaining, &code_point);
        if (consumed < 0 || code_point == 0)
            throw Error{ErrorCategory::validation,
                        "export contains invalid UTF-8 or NUL in " + std::string{description}};
        cursor += consumed;
        remaining -= consumed;
    }
}

void validate_result(const db::SqlResult& result, const ExportOptions& options) {
    if (options.destination.empty())
        throw Error{ErrorCategory::validation, "export destination cannot be empty"};
    if (result.columns.empty())
        throw Error{ErrorCategory::validation, "export result has no columns"};
    if (result.truncated)
        throw Error{ErrorCategory::validation,
                    "export result exceeded its row limit; increase the limit before exporting"};
    validate_utf8(options.title, "the title");
    for (const auto& column : result.columns)
        validate_utf8(column, "a column name");
    for (const auto& row : result.rows) {
        if (row.size() != result.columns.size())
            throw Error{ErrorCategory::validation, "export row width does not match its columns"};
        for (std::size_t index = 0; index < row.size(); ++index) {
            const auto& value = row.at(index);
            if (std::holds_alternative<std::string>(value.storage()))
                validate_utf8(value.as_string(), "a text value");
        }
    }
}

auto invariant_number(double value) -> std::string {
    std::array<char, 128> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                            std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (error != std::errc{})
        throw Error{ErrorCategory::validation, "cannot represent a floating-point export value"};
    return {buffer.data(), end};
}

auto hex_blob(std::span<const std::byte> blob) -> std::string {
    constexpr std::string_view digits{"0123456789ABCDEF"};
    std::string result;
    result.reserve(blob.size() * 2);
    for (const auto byte : blob) {
        const auto value = std::to_integer<unsigned int>(byte);
        result += digits[value >> 4U];
        result += digits[value & 0x0FU];
    }
    return result;
}

auto machine_text(const db::Value& value) -> std::string {
    if (value.is_null())
        return {};
    if (std::holds_alternative<std::int64_t>(value.storage()))
        return std::to_string(value.as_int());
    if (std::holds_alternative<double>(value.storage()))
        return invariant_number(value.as_double());
    if (std::holds_alternative<std::string>(value.storage()))
        return value.as_string();
    return "X'" + hex_blob(value.as_blob()) + "'";
}

auto flattened_text(std::string text) -> std::string {
    std::ranges::replace_if(text, [](unsigned char character) { return character < 0x20U || character == 0x7FU; }, ' ');
    return text;
}

auto csv_field(std::string_view value) -> std::string {
    if (value.find_first_of(",\"\r\n") == std::string_view::npos)
        return std::string{value};
    std::string result{"\""};
    for (const auto character : value) {
        result += character;
        if (character == '"')
            result += '"';
    }
    result += '"';
    return result;
}

void write_csv(std::ostream& output, const db::SqlResult& result) {
    for (std::size_t index = 0; index < result.columns.size(); ++index)
        output << (index == 0 ? "" : ",") << csv_field(result.columns[index]);
    output << "\r\n";
    for (const auto& row : result.rows) {
        for (std::size_t index = 0; index < row.size(); ++index)
            output << (index == 0 ? "" : ",") << csv_field(machine_text(row.at(index)));
        output << "\r\n";
    }
}

auto json_value(const db::Value& value) -> nlohmann::json {
    if (value.is_null())
        return nullptr;
    if (std::holds_alternative<std::int64_t>(value.storage()))
        return value.as_int();
    if (std::holds_alternative<double>(value.storage()))
        return value.as_double();
    if (std::holds_alternative<std::string>(value.storage()))
        return value.as_string();
    return nlohmann::json{{"$blob", hex_blob(value.as_blob())}};
}

void write_json(std::ostream& output, const db::SqlResult& result) {
    nlohmann::json document{{"columns", result.columns}, {"rows", nlohmann::json::array()}};
    for (const auto& row : result.rows) {
        auto values = nlohmann::json::array();
        for (std::size_t index = 0; index < row.size(); ++index)
            values.push_back(json_value(row.at(index)));
        document["rows"].push_back(std::move(values));
    }
    output << std::setw(2) << document << '\n';
}

auto escaped_plain(std::string value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        switch (character) {
        case '\\':
            result += "\\\\";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\n':
            result += "\\n";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

void write_text(std::ostream& output, const db::SqlResult& result, const core::LocaleFormatter& formatter) {
    for (std::size_t index = 0; index < result.columns.size(); ++index)
        output << (index == 0 ? "" : "\t") << escaped_plain(result.columns[index]);
    output << '\n';
    for (const auto& row : result.rows) {
        for (std::size_t index = 0; index < row.size(); ++index)
            output << (index == 0 ? "" : "\t") << escaped_plain(detail::display_text(row.at(index), formatter));
        output << '\n';
    }
}

auto html_escape(std::string_view value) -> std::string {
    std::string result;
    for (const auto character : value) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

void write_html(std::ostream& output, const db::SqlResult& result, const ExportOptions& options,
                const core::LocaleFormatter& formatter) {
    output << "<!doctype html>\n<html lang=\"" << html_escape(options.locale)
           << "\">\n<head><meta charset=\"utf-8\"><title>" << html_escape(options.title)
           << "</title><style>body{font-family:system-ui,sans-serif;margin:2rem}table{border-collapse:collapse}"
              "th,td{border:1px solid #999;padding:.35rem .55rem;text-align:left}th{background:#eee}</style></head>\n"
              "<body><h1>"
           << html_escape(options.title) << "</h1><table><thead><tr>";
    for (const auto& column : result.columns)
        output << "<th scope=\"col\">" << html_escape(column) << "</th>";
    output << "</tr></thead><tbody>\n";
    for (const auto& row : result.rows) {
        output << "<tr>";
        for (std::size_t index = 0; index < row.size(); ++index)
            output << "<td>" << html_escape(detail::display_text(row.at(index), formatter)) << "</td>";
        output << "</tr>\n";
    }
    output << "</tbody></table></body></html>\n";
}

class PendingExport {
  public:
    explicit PendingExport(const ExportOptions& options)
        : destination_{options.destination}, overwrite_{options.overwrite} {
        std::error_code error;
        if (std::filesystem::exists(destination_, error) && !overwrite_)
            throw Error{ErrorCategory::io, "export destination already exists: " + path_text(destination_)};
        if (error)
            throw Error{ErrorCategory::io, "cannot inspect export destination: " + error.message()};
        auto parent = destination_.parent_path();
        if (parent.empty())
            parent = ".";
        if (!std::filesystem::is_directory(parent, error) || error)
            throw Error{ErrorCategory::io, "export destination directory does not exist: " + path_text(parent)};
        for (int suffix = 0; suffix < 1'000; ++suffix) {
            temporary_ = sibling_artifact_path(parent, destination_, ".vulpes-tmp-", suffix);
            if (!std::filesystem::exists(temporary_, error) && !error)
                return;
            error.clear();
        }
        throw Error{ErrorCategory::io, "cannot allocate a temporary export file"};
    }

    ~PendingExport() {
        if (!committed_) {
            std::error_code ignored;
            std::filesystem::remove(temporary_, ignored);
        }
    }

    PendingExport(const PendingExport&) = delete;
    auto operator=(const PendingExport&) -> PendingExport& = delete;

    [[nodiscard]] auto path() const -> const std::filesystem::path& { return temporary_; }

    void commit() {
        std::error_code error;
        const bool destination_exists = std::filesystem::exists(destination_, error);
        if (error)
            throw Error{ErrorCategory::io, "cannot inspect export destination: " + error.message()};
        if (destination_exists && !overwrite_)
            throw Error{ErrorCategory::io, "export destination appeared while writing: " + path_text(destination_)};

        if (destination_exists) {
            auto parent = destination_.parent_path();
            if (parent.empty())
                parent = ".";
            for (int suffix = 0; suffix < 1'000; ++suffix) {
                backup_ = sibling_artifact_path(parent, destination_, ".vulpes-backup-", suffix);
                if (!std::filesystem::exists(backup_, error) && !error)
                    break;
                backup_.clear();
                error.clear();
            }
            if (backup_.empty())
                throw Error{ErrorCategory::io, "cannot allocate a temporary export backup"};
            std::filesystem::rename(destination_, backup_, error);
            if (error)
                throw Error{ErrorCategory::io, "cannot preserve the previous export: " + error.message()};
        }

        std::filesystem::rename(temporary_, destination_, error);
        if (error) {
            if (!backup_.empty()) {
                std::error_code restore_error;
                std::filesystem::rename(backup_, destination_, restore_error);
            }
            throw Error{ErrorCategory::io, "cannot finish export: " + error.message()};
        }
        committed_ = true;
        if (!backup_.empty()) {
            std::filesystem::remove(backup_, error);
            if (error)
                throw Error{ErrorCategory::io,
                            "export finished but its previous-file backup could not be removed: " + error.message()};
        }
    }

  private:
    std::filesystem::path destination_;
    std::filesystem::path temporary_;
    std::filesystem::path backup_;
    bool overwrite_{};
    bool committed_{};
};

void write_stream_file(const std::filesystem::path& path, const std::function<void(std::ostream&)>& write) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
        throw Error{ErrorCategory::io, "cannot create export file: " + path_text(path)};
    write(output);
    output.close();
    if (!output)
        throw Error{ErrorCategory::io, "cannot finish export file: " + path_text(path)};
}

} // namespace

auto detail::display_text(const db::Value& value, const core::LocaleFormatter& formatter) -> std::string {
    if (value.is_null())
        return "NULL";
    if (std::holds_alternative<std::int64_t>(value.storage()))
        return formatter.number(value.as_int());
    if (std::holds_alternative<double>(value.storage()))
        return formatter.number(value.as_double());
    return flattened_text(machine_text(value));
}

auto parse_export_format(std::string_view name) -> ExportFormat {
    const auto normalized = lowercase_ascii(std::string{name});
    if (normalized == "csv")
        return ExportFormat::csv;
    if (normalized == "json")
        return ExportFormat::json;
    if (normalized == "text" || normalized == "txt")
        return ExportFormat::text;
    if (normalized == "html" || normalized == "htm")
        return ExportFormat::html;
    if (normalized == "pdf")
        return ExportFormat::pdf;
    throw Error{ErrorCategory::validation, "unknown export format: " + std::string{name}};
}

auto infer_export_format(const std::filesystem::path& destination) -> ExportFormat {
    auto extension = destination.extension().string();
    if (!extension.empty() && extension.front() == '.')
        extension.erase(extension.begin());
    if (extension.empty())
        throw Error{ErrorCategory::validation,
                    "export format cannot be inferred from a destination without an extension"};
    return parse_export_format(extension);
}

auto export_format_name(ExportFormat format) noexcept -> std::string_view {
    switch (format) {
    case ExportFormat::csv:
        return "csv";
    case ExportFormat::json:
        return "json";
    case ExportFormat::text:
        return "text";
    case ExportFormat::html:
        return "html";
    case ExportFormat::pdf:
        return "pdf";
    }
    return {};
}

auto export_result(const db::SqlResult& result, const ExportOptions& options) -> ExportSummary {
    validate_result(result, options);
    const core::LocaleFormatter formatter{options.locale};
    PendingExport pending{options};
    if (options.format == ExportFormat::pdf) {
        detail::write_pdf(pending.path(), result, options.title, formatter);
    } else {
        write_stream_file(pending.path(), [&](std::ostream& output) {
            switch (options.format) {
            case ExportFormat::csv:
                write_csv(output, result);
                break;
            case ExportFormat::json:
                write_json(output, result);
                break;
            case ExportFormat::text:
                write_text(output, result, formatter);
                break;
            case ExportFormat::html:
                write_html(output, result, options, formatter);
                break;
            case ExportFormat::pdf:
                break;
            }
        });
    }
    pending.commit();
    return {.destination = options.destination, .rows = result.rows.size(), .format = options.format};
}

auto export_query(db::Database& database, std::string_view query, std::size_t row_limit, const ExportOptions& options)
    -> ExportSummary {
    return export_result(database.run_query(query, row_limit), options);
}

} // namespace vulpes::report
