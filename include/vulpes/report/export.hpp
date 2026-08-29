#pragma once

#include "vulpes/db/database.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace vulpes::report {

enum class ExportFormat { csv, json, text, html, pdf };

struct ExportOptions {
    std::filesystem::path destination;
    ExportFormat format{ExportFormat::csv};
    bool overwrite{false};
    std::string title{"Vulpes export"};
    std::string locale{"en"};
};

struct ExportSummary {
    std::filesystem::path destination;
    std::size_t rows{};
    ExportFormat format{ExportFormat::csv};
};

[[nodiscard]] auto parse_export_format(std::string_view name) -> ExportFormat;
[[nodiscard]] auto infer_export_format(const std::filesystem::path& destination) -> ExportFormat;
[[nodiscard]] auto export_format_name(ExportFormat format) noexcept -> std::string_view;

// Export is intentionally independent of terminal widgets. Machine-readable
// formats preserve SQLite scalar types; human-readable formats apply the
// requested locale to numeric display only.
[[nodiscard]] auto export_result(const db::SqlResult& result, const ExportOptions& options) -> ExportSummary;
[[nodiscard]] auto export_query(db::Database& database, std::string_view query, std::size_t row_limit,
                                const ExportOptions& options) -> ExportSummary;

} // namespace vulpes::report
