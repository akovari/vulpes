#include "vulpes/appmeta/metadata.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/core/formatting.hpp"
#include "vulpes/db/schema.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace vulpes::appmeta {
namespace {

[[nodiscard]] auto has_field(const db::TableSchema& table, std::string_view name) -> bool {
    return std::ranges::find(table.fields, name, &db::FieldSchema::name) != table.fields.end();
}

[[nodiscard]] auto has_text_affinity(const db::FieldSchema& field) -> bool {
    auto type = field.declared_type;
    std::ranges::transform(type, type.begin(),
                           [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
    return type.find("CHAR") != std::string::npos || type.find("CLOB") != std::string::npos ||
           type.find("TEXT") != std::string::npos;
}

[[nodiscard]] auto referenced_table(std::span<const db::TableSchema> schema, const db::ForeignKeySchema& foreign_key)
    -> const db::TableSchema* {
    const auto table = std::ranges::find(schema, foreign_key.referenced_table, &db::TableSchema::name);
    return table == schema.end() ? nullptr : &*table;
}

void validate_currency(const FieldMetadata& field, std::string_view table) {
    if (field.format != FieldFormat::currency)
        return;
    if (!field.currency_code || field.currency_code->size() != 3 ||
        !std::ranges::all_of(*field.currency_code, [](unsigned char character) {
            return character >= static_cast<unsigned char>('A') && character <= static_cast<unsigned char>('Z');
        })) {
        throw Error{ErrorCategory::metadata,
                    "currency field requires an uppercase ISO 4217 code: " + std::string{table} + "." + field.name};
    }
}

void validate_temporal(const FieldMetadata& field, const db::FieldSchema& schema_field, std::string_view table) {
    const bool temporal = field.format == FieldFormat::date || field.format == FieldFormat::time ||
                          field.format == FieldFormat::date_time;
    if (temporal && !has_text_affinity(schema_field)) {
        throw Error{ErrorCategory::metadata, "date/time annotations require a TEXT-affinity SQLite field: " +
                                                 std::string{table} + "." + field.name};
    }
    if (field.format == FieldFormat::date_time && (!field.time_zone || field.time_zone->empty())) {
        throw Error{ErrorCategory::metadata,
                    "date_time field requires an IANA time zone: " + std::string{table} + "." + field.name};
    }
    if (field.format == FieldFormat::date_time) {
        try {
            static_cast<void>(core::LocaleFormatter{"en", *field.time_zone});
        } catch (const Error&) {
            throw Error{ErrorCategory::metadata,
                        "date_time field has an unknown IANA time zone: " + std::string{table} + "." + field.name};
        }
    }
}

} // namespace

auto TableMetadata::field(std::string_view field_name) const noexcept -> const FieldMetadata* {
    const auto result = std::ranges::find(fields, field_name, &FieldMetadata::name);
    return result == fields.end() ? nullptr : &*result;
}

auto ApplicationMetadata::table(std::string_view table_name) const noexcept -> const TableMetadata* {
    const auto result = std::ranges::find(tables, table_name, &TableMetadata::name);
    return result == tables.end() ? nullptr : &*result;
}

void ApplicationMetadata::validate(std::span<const db::TableSchema> schema) const {
    std::set<std::string, std::less<>> table_names;
    for (const auto& table_metadata : tables) {
        if (table_metadata.name.empty() || !table_names.insert(table_metadata.name).second)
            throw Error{ErrorCategory::metadata, "application metadata contains an empty or duplicate table name"};
        const auto table_schema = std::ranges::find(schema, table_metadata.name, &db::TableSchema::name);
        if (table_schema == schema.end())
            throw Error{ErrorCategory::metadata,
                        "metadata references an unknown table or view: " + table_metadata.name};
        if (table_metadata.label && table_metadata.label->empty())
            throw Error{ErrorCategory::metadata, "table label cannot be empty: " + table_metadata.name};

        std::set<std::string, std::less<>> field_names;
        for (const auto& field_metadata : table_metadata.fields) {
            if (field_metadata.name.empty() || !field_names.insert(field_metadata.name).second)
                throw Error{ErrorCategory::metadata,
                            "metadata contains an empty or duplicate field in table: " + table_metadata.name};
            const auto schema_field =
                std::ranges::find(table_schema->fields, field_metadata.name, &db::FieldSchema::name);
            if (schema_field == table_schema->fields.end())
                throw Error{ErrorCategory::metadata,
                            "metadata references an unknown field: " + table_metadata.name + "." + field_metadata.name};
            if (field_metadata.label && field_metadata.label->empty())
                throw Error{ErrorCategory::metadata,
                            "field label cannot be empty: " + table_metadata.name + "." + field_metadata.name};
            validate_currency(field_metadata, table_metadata.name);
            validate_temporal(field_metadata, *schema_field, table_metadata.name);

            if (!field_metadata.lookup)
                continue;
            if (field_metadata.lookup->result_limit == 0 || field_metadata.lookup->result_limit > 1000)
                throw Error{ErrorCategory::metadata, "lookup result limit must be between 1 and 1000: " +
                                                         table_metadata.name + "." + field_metadata.name};
            const auto foreign_key =
                std::ranges::find(table_schema->foreign_keys, field_metadata.name, &db::ForeignKeySchema::field);
            if (foreign_key == table_schema->foreign_keys.end())
                throw Error{ErrorCategory::metadata, "lookup metadata requires a foreign key: " + table_metadata.name +
                                                         "." + field_metadata.name};
            const auto* related = referenced_table(schema, *foreign_key);
            if (related == nullptr)
                throw Error{ErrorCategory::metadata,
                            "lookup references an unavailable table: " + foreign_key->referenced_table};
            if (field_metadata.lookup->display_field && !has_field(*related, *field_metadata.lookup->display_field)) {
                throw Error{ErrorCategory::metadata, "lookup display field does not exist: " + related->name + "." +
                                                         *field_metadata.lookup->display_field};
            }
            for (const auto& search_field : field_metadata.lookup->search_fields) {
                if (!has_field(*related, search_field))
                    throw Error{ErrorCategory::metadata,
                                "lookup search field does not exist: " + related->name + "." + search_field};
            }
        }
    }
}

} // namespace vulpes::appmeta
