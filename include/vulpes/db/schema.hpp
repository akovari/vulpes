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
    bool unique{false};
    std::optional<std::string> default_expression;
};

struct ForeignKeySchema {
    std::string field;
    std::string referenced_table;
    std::string referenced_field;
};

struct TableSchema {
    std::string name;
    bool is_view{false};
    std::vector<FieldSchema> fields;
    std::vector<ForeignKeySchema> foreign_keys;
};

[[nodiscard]] auto inspect_schema(Database& database) -> std::vector<TableSchema>;

} // namespace vulpes::db

