#pragma once

#include <optional>
#include <string>
#include <vector>

namespace vulpes::db {

class Database;

struct FieldSchema {
    std::string name;
    std::string declared_type;
    bool nullable{true};
    bool primary_key{false};
    int primary_key_position{0};
    bool unique{false};
    bool hidden{false};
    bool generated{false};
    std::optional<std::string> default_expression;
};

struct ForeignKeySchema {
    std::string field;
    std::string referenced_table;
    std::string referenced_field;
    std::string on_update;
    std::string on_delete;
};

struct IndexSchema {
    std::string name;
    bool unique{false};
    std::vector<std::string> fields;
};

struct TableSchema {
    std::string name;
    bool is_view{false};
    bool without_rowid{false};
    std::vector<FieldSchema> fields;
    std::vector<ForeignKeySchema> foreign_keys;
    std::vector<IndexSchema> indexes;

    [[nodiscard]] auto primary_key_fields() const -> std::vector<std::string>;
};

[[nodiscard]] auto inspect_schema(Database& database) -> std::vector<TableSchema>;

} // namespace vulpes::db
