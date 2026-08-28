#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vulpes::db {
struct TableSchema;
}

namespace vulpes::appmeta {

// Presentation annotations never replace SQLite schema facts. In particular,
// they cannot make generated columns or unstable rows writable.
enum class FieldFormat { automatic, text, number, boolean, date, time, date_time, currency };

struct LookupMetadata {
    std::optional<std::string> display_field;
    std::vector<std::string> search_fields;
    std::size_t result_limit{100};
    bool allow_drill_down{true};
};

struct FieldMetadata {
    std::string name;
    std::optional<std::string> label;
    std::optional<std::size_t> order;
    std::optional<bool> visible;
    std::optional<bool> read_only;
    FieldFormat format{FieldFormat::automatic};
    std::optional<std::string> currency_code;
    std::optional<std::string> time_zone;
    std::optional<LookupMetadata> lookup;
};

struct TableMetadata {
    std::string name;
    std::optional<std::string> label;
    std::vector<FieldMetadata> fields;

    [[nodiscard]] auto field(std::string_view field_name) const noexcept -> const FieldMetadata*;
};

class ApplicationMetadata {
  public:
    std::vector<TableMetadata> tables;

    [[nodiscard]] auto table(std::string_view table_name) const noexcept -> const TableMetadata*;
    void validate(std::span<const db::TableSchema> schema) const;
};

} // namespace vulpes::appmeta
