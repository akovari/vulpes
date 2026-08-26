#include "vulpes/db/schema.hpp"

#include "vulpes/db/database.hpp"
#include "vulpes/db/identifier.hpp"

#include <algorithm>

namespace vulpes::db {

auto TableSchema::primary_key_fields() const -> std::vector<std::string> {
    std::vector<const FieldSchema*> key_fields;
    for (const auto& field : fields) {
        if (field.primary_key) key_fields.push_back(&field);
    }
    std::ranges::sort(key_fields, {}, &FieldSchema::primary_key_position);
    std::vector<std::string> result;
    result.reserve(key_fields.size());
    for (const auto* field : key_fields) result.push_back(field->name);
    return result;
}

auto inspect_schema(Database& database) -> std::vector<TableSchema> {
    std::vector<TableSchema> tables;
    auto objects = database.prepare(
        "SELECT name, type FROM sqlite_schema "
        "WHERE type IN ('table', 'view') AND name NOT LIKE 'sqlite_%' ORDER BY name");

    while (objects.step()) {
        TableSchema table;
        table.name = objects.column(0).as_string();
        table.is_view = objects.column(1).as_string() == "view";

        auto columns = database.prepare("PRAGMA table_xinfo(" + detail::quote_identifier(table.name) + ")");
        while (columns.step()) {
            FieldSchema field;
            field.name = columns.column(1).as_string();
            field.declared_type = columns.column(2).as_string();
            field.nullable = columns.column(3).as_int() == 0;
            field.primary_key = columns.column(5).as_int() != 0;
            field.primary_key_position = static_cast<int>(columns.column(5).as_int());
            const auto hidden_value = columns.column_count() > 6 ? columns.column(6).as_int() : 0;
            field.hidden = hidden_value == 1;
            field.generated = hidden_value == 2 || hidden_value == 3;
            if (!columns.column(4).is_null()) field.default_expression = columns.column(4).as_string();
            table.fields.push_back(std::move(field));
        }

        auto foreign_keys = database.prepare("PRAGMA foreign_key_list(" + detail::quote_identifier(table.name) + ")");
        while (foreign_keys.step()) {
            table.foreign_keys.push_back(ForeignKeySchema{
                foreign_keys.column(3).as_string(), foreign_keys.column(2).as_string(),
                foreign_keys.column(4).as_string(), foreign_keys.column(5).as_string(), foreign_keys.column(6).as_string()});
        }

        auto indexes = database.prepare("PRAGMA index_list(" + detail::quote_identifier(table.name) + ")");
        while (indexes.step()) {
            IndexSchema index;
            index.name = indexes.column(1).as_string();
            index.unique = indexes.column(2).as_int() != 0;
            auto fields = database.prepare("PRAGMA index_info(" + detail::quote_identifier(index.name) + ")");
            while (fields.step()) {
                if (!fields.column(2).is_null()) index.fields.push_back(fields.column(2).as_string());
            }
            if (index.unique && index.fields.size() == 1) {
                for (auto& field : table.fields) {
                    if (field.name == index.fields.front()) field.unique = true;
                }
            }
            table.indexes.push_back(std::move(index));
        }
        tables.push_back(std::move(table));
    }
    return tables;
}

} // namespace vulpes::db
