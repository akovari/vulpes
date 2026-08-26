#include "vulpes/db/schema.hpp"

#include "vulpes/db/database.hpp"

#include <unordered_map>

namespace vulpes::db {
namespace {

auto quote_identifier(const std::string& identifier) -> std::string {
    std::string result{"\""};
    for (const char character : identifier) {
        result += character;
        if (character == '\"') result += '\"';
    }
    return result + '\"';
}

} // namespace

auto inspect_schema(Database& database) -> std::vector<TableSchema> {
    std::vector<TableSchema> tables;
    auto objects = database.prepare(
        "SELECT name, type FROM sqlite_schema "
        "WHERE type IN ('table', 'view') AND name NOT LIKE 'sqlite_%' ORDER BY name");

    while (objects.step()) {
        TableSchema table;
        table.name = objects.column(0).as_string();
        table.is_view = objects.column(1).as_string() == "view";

        auto columns = database.prepare("PRAGMA table_xinfo(" + quote_identifier(table.name) + ")");
        while (columns.step()) {
            FieldSchema field;
            field.name = columns.column(1).as_string();
            field.declared_type = columns.column(2).as_string();
            field.nullable = columns.column(3).as_int() == 0;
            field.primary_key = columns.column(5).as_int() != 0;
            if (!columns.column(4).is_null()) field.default_expression = columns.column(4).as_string();
            table.fields.push_back(std::move(field));
        }

        auto foreign_keys = database.prepare("PRAGMA foreign_key_list(" + quote_identifier(table.name) + ")");
        while (foreign_keys.step()) {
            table.foreign_keys.push_back(ForeignKeySchema{
                foreign_keys.column(3).as_string(), foreign_keys.column(2).as_string(),
                foreign_keys.column(4).as_string()});
        }
        tables.push_back(std::move(table));
    }
    return tables;
}

} // namespace vulpes::db

