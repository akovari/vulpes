#include "vulpes/appmeta/loader.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/schema.hpp"
#include "vulpes/db/transaction.hpp"

#include <algorithm>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace vulpes::appmeta {
namespace {

[[nodiscard]] auto table_exists(db::Database& database, std::string_view name) -> bool {
    auto statement = database.prepare("SELECT 1 FROM sqlite_schema WHERE type = 'table' AND name = ? LIMIT 1");
    statement.bind(1, db::Value{std::string{name}});
    return statement.step();
}

[[nodiscard]] auto optional_string(const db::Value& value) -> std::optional<std::string> {
    return value.is_null() ? std::nullopt : std::optional{value.as_string()};
}

[[nodiscard]] auto optional_size(const db::Value& value) -> std::optional<std::size_t> {
    if (value.is_null())
        return std::nullopt;
    const auto number = value.as_int();
    if (number < 0)
        throw Error{ErrorCategory::metadata, "application metadata position cannot be negative"};
    return static_cast<std::size_t>(number);
}

[[nodiscard]] auto optional_bool(const db::Value& value) -> std::optional<bool> {
    if (value.is_null())
        return std::nullopt;
    const auto number = value.as_int();
    if (number != 0 && number != 1)
        throw Error{ErrorCategory::metadata, "application metadata boolean must be 0 or 1"};
    return number != 0;
}

[[nodiscard]] auto parse_format(std::string_view format) -> FieldFormat {
    if (format.empty() || format == "automatic")
        return FieldFormat::automatic;
    if (format == "text")
        return FieldFormat::text;
    if (format == "number")
        return FieldFormat::number;
    if (format == "boolean")
        return FieldFormat::boolean;
    if (format == "date")
        return FieldFormat::date;
    if (format == "time")
        return FieldFormat::time;
    if (format == "date_time")
        return FieldFormat::date_time;
    if (format == "currency")
        return FieldFormat::currency;
    throw Error{ErrorCategory::metadata, "unknown application field format: " + std::string{format}};
}

[[nodiscard]] auto parse_search_fields(const db::Value& value) -> std::vector<std::string> {
    if (value.is_null())
        return {};
    try {
        const auto document = nlohmann::json::parse(value.as_string());
        if (!document.is_array())
            throw Error{ErrorCategory::metadata, "lookup search fields must be a JSON array"};
        std::vector<std::string> fields;
        fields.reserve(document.size());
        for (const auto& field : document) {
            if (!field.is_string() || field.get_ref<const std::string&>().empty())
                throw Error{ErrorCategory::metadata, "lookup search fields must contain non-empty strings"};
            fields.push_back(field.get<std::string>());
        }
        return fields;
    } catch (const Error&) {
        throw;
    } catch (const nlohmann::json::exception& error) {
        throw Error{ErrorCategory::metadata, "invalid lookup search-fields JSON: " + std::string{error.what()}};
    }
}

[[nodiscard]] auto parse_filter_operator(std::string_view source) -> model::FilterOperator {
    if (source == "equal")
        return model::FilterOperator::equal;
    if (source == "not_equal")
        return model::FilterOperator::not_equal;
    if (source == "less")
        return model::FilterOperator::less;
    if (source == "less_equal")
        return model::FilterOperator::less_equal;
    if (source == "greater")
        return model::FilterOperator::greater;
    if (source == "greater_equal")
        return model::FilterOperator::greater_equal;
    throw Error{ErrorCategory::metadata, "unknown application view filter comparison: " + std::string{source}};
}

[[nodiscard]] auto parse_filter_value(const db::Value& kind, const db::Value& integer_value,
                                      const db::Value& real_value, const db::Value& text_value,
                                      const db::Value& blob_value) -> db::Value {
    const auto require_null = [](const db::Value& value) {
        if (!value.is_null())
            throw Error{ErrorCategory::metadata, "application view filter has an invalid typed value"};
    };
    if (!std::holds_alternative<std::string>(kind.storage()))
        throw Error{ErrorCategory::metadata, "application view filter value type must be text"};
    const auto value_kind = kind.as_string();
    if (value_kind == "null") {
        require_null(integer_value);
        require_null(real_value);
        require_null(text_value);
        require_null(blob_value);
        return {};
    }
    if (value_kind == "integer") {
        require_null(real_value);
        require_null(text_value);
        require_null(blob_value);
        if (!std::holds_alternative<std::int64_t>(integer_value.storage()))
            throw Error{ErrorCategory::metadata, "application view filter integer value is invalid"};
        return integer_value;
    }
    if (value_kind == "real") {
        require_null(integer_value);
        require_null(text_value);
        require_null(blob_value);
        if (!std::holds_alternative<double>(real_value.storage()))
            throw Error{ErrorCategory::metadata, "application view filter real value is invalid"};
        return real_value;
    }
    if (value_kind == "text") {
        require_null(integer_value);
        require_null(real_value);
        require_null(blob_value);
        if (!std::holds_alternative<std::string>(text_value.storage()))
            throw Error{ErrorCategory::metadata, "application view filter text value is invalid"};
        return text_value;
    }
    if (value_kind == "blob") {
        require_null(integer_value);
        require_null(real_value);
        require_null(text_value);
        if (!std::holds_alternative<db::Blob>(blob_value.storage()))
            throw Error{ErrorCategory::metadata, "application view filter blob value is invalid"};
        return blob_value;
    }
    throw Error{ErrorCategory::metadata, "unknown application view filter value type: " + value_kind};
}

void create_version_one(db::Database& database) {
    database.execute("CREATE TABLE _app_schema("
                     "singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
                     "version INTEGER NOT NULL CHECK(version > 0));"
                     "INSERT INTO _app_schema(singleton, version) VALUES(1, 1);"
                     "CREATE TABLE _app_settings("
                     "key TEXT PRIMARY KEY, value TEXT NOT NULL);"
                     "CREATE TABLE _app_forms("
                     "name TEXT PRIMARY KEY, table_name TEXT NOT NULL, label TEXT,"
                     "is_default INTEGER NOT NULL DEFAULT 0 CHECK(is_default IN (0, 1)));"
                     "CREATE UNIQUE INDEX _app_forms_one_default_per_table "
                     "ON _app_forms(table_name) WHERE is_default = 1;"
                     "CREATE TABLE _app_form_fields("
                     "form_name TEXT NOT NULL REFERENCES _app_forms(name) ON DELETE CASCADE,"
                     "field_name TEXT NOT NULL, label TEXT, position INTEGER CHECK(position >= 0),"
                     "visible INTEGER CHECK(visible IN (0, 1)), read_only INTEGER CHECK(read_only IN (0, 1)),"
                     "format TEXT NOT NULL DEFAULT 'automatic', currency_code TEXT, time_zone TEXT,"
                     "lookup_display_field TEXT, lookup_search_fields TEXT,"
                     "lookup_result_limit INTEGER CHECK(lookup_result_limit BETWEEN 1 AND 1000),"
                     "lookup_allow_drill_down INTEGER CHECK(lookup_allow_drill_down IN (0, 1)),"
                     "PRIMARY KEY(form_name, field_name));");
}

void migrate_one_to_two(db::Database& database) {
    database.execute("CREATE TABLE _app_views("
                     "name TEXT PRIMARY KEY, table_name TEXT NOT NULL, label TEXT,"
                     "form_name TEXT REFERENCES _app_forms(name));"
                     "CREATE TABLE _app_reports("
                     "name TEXT PRIMARY KEY, label TEXT NOT NULL, sql TEXT NOT NULL,"
                     "row_limit INTEGER NOT NULL DEFAULT 1000 CHECK(row_limit BETWEEN 1 AND 100000));"
                     "CREATE TABLE _app_commands("
                     "name TEXT PRIMARY KEY, label TEXT, command TEXT NOT NULL);"
                     "CREATE TABLE _app_menus("
                     "name TEXT PRIMARY KEY, label TEXT NOT NULL, position INTEGER NOT NULL CHECK(position >= 0));"
                     "CREATE TABLE _app_menu_items("
                     "menu_name TEXT NOT NULL REFERENCES _app_menus(name) ON DELETE CASCADE,"
                     "position INTEGER NOT NULL CHECK(position >= 0), label TEXT NOT NULL,"
                     "command_name TEXT NOT NULL REFERENCES _app_commands(name),"
                     "PRIMARY KEY(menu_name, position));"
                     "UPDATE _app_schema SET version = 2 WHERE singleton = 1;");
}

void migrate_two_to_three(db::Database& database) {
    database.execute(
        "CREATE TABLE _app_scripts("
        "name TEXT PRIMARY KEY,"
        "hook TEXT NOT NULL CHECK(hook IN ('on_open', 'before_insert', 'before_update', 'after_update', "
        "'before_delete', 'on_command')),"
        "table_name TEXT, command_name TEXT, source TEXT NOT NULL CHECK(length(source) BETWEEN 1 AND 65536),"
        "position INTEGER NOT NULL DEFAULT 0 CHECK(position BETWEEN 0 AND 100000),"
        "CHECK((hook = 'on_open' AND table_name IS NULL AND command_name IS NULL) OR "
        "(hook IN ('before_insert', 'before_update', 'after_update', 'before_delete') "
        "AND table_name IS NOT NULL AND command_name IS NULL) OR "
        "(hook = 'on_command' AND table_name IS NULL)));"
        "UPDATE _app_schema SET version = 3 WHERE singleton = 1;");
}

void migrate_three_to_four(db::Database& database) {
    database.execute("CREATE TABLE _app_screens("
                     "name TEXT PRIMARY KEY, label TEXT NOT NULL, description TEXT,"
                     "is_default INTEGER NOT NULL DEFAULT 0 CHECK(is_default IN (0, 1)));"
                     "CREATE UNIQUE INDEX _app_screens_one_default "
                     "ON _app_screens(is_default) WHERE is_default = 1;"
                     "CREATE TABLE _app_screen_items("
                     "screen_name TEXT NOT NULL REFERENCES _app_screens(name) ON DELETE CASCADE,"
                     "position INTEGER NOT NULL CHECK(position >= 0), label TEXT NOT NULL, description TEXT,"
                     "command_name TEXT NOT NULL REFERENCES _app_commands(name),"
                     "PRIMARY KEY(screen_name, position));"
                     "UPDATE _app_schema SET version = 4 WHERE singleton = 1;");
}

void migrate_four_to_five(db::Database& database) {
    database.execute(
        "ALTER TABLE _app_views ADD COLUMN default_filter_name TEXT;"
        "CREATE TABLE _app_view_filters("
        "view_name TEXT NOT NULL REFERENCES _app_views(name) ON DELETE CASCADE,"
        "name TEXT NOT NULL CHECK(length(name) BETWEEN 1 AND 128),"
        "position INTEGER NOT NULL CHECK(position BETWEEN 0 AND 100000), search_text TEXT,"
        "PRIMARY KEY(view_name, name));"
        "CREATE TABLE _app_view_filter_terms("
        "view_name TEXT NOT NULL, filter_name TEXT NOT NULL, position INTEGER NOT NULL CHECK(position BETWEEN 0 AND "
        "1000),"
        "field_name TEXT NOT NULL, comparison TEXT NOT NULL CHECK(comparison IN "
        "('equal', 'not_equal', 'less', 'less_equal', 'greater', 'greater_equal')),"
        "value_kind TEXT NOT NULL CHECK(value_kind IN ('null', 'integer', 'real', 'text', 'blob')),"
        "integer_value INTEGER, real_value REAL, text_value TEXT, blob_value BLOB,"
        "CHECK((value_kind = 'null' AND integer_value IS NULL AND real_value IS NULL AND text_value IS NULL "
        "AND blob_value IS NULL) OR (value_kind = 'integer' AND integer_value IS NOT NULL AND real_value IS NULL "
        "AND text_value IS NULL AND blob_value IS NULL) OR (value_kind = 'real' AND integer_value IS NULL "
        "AND real_value IS NOT NULL AND text_value IS NULL AND blob_value IS NULL) OR (value_kind = 'text' "
        "AND integer_value IS NULL AND real_value IS NULL AND text_value IS NOT NULL AND blob_value IS NULL) "
        "OR (value_kind = 'blob' AND integer_value IS NULL AND real_value IS NULL AND text_value IS NULL "
        "AND blob_value IS NOT NULL)),"
        "PRIMARY KEY(view_name, filter_name, position), FOREIGN KEY(view_name, filter_name) "
        "REFERENCES _app_view_filters(view_name, name) ON DELETE CASCADE);"
        "UPDATE _app_schema SET version = 5 WHERE singleton = 1;");
}

void load_settings(db::Database& database, ApplicationDefinition& definition) {
    auto statement = database.prepare("SELECT key, value FROM _app_settings ORDER BY key");
    while (statement.step())
        definition.settings.push_back(
            {.key = statement.column(0).as_string(), .value = statement.column(1).as_string()});
}

void load_forms(db::Database& database, ApplicationDefinition& definition) {
    auto forms = database.prepare("SELECT name, table_name, label, is_default FROM _app_forms ORDER BY name");
    while (forms.step()) {
        FormDefinition form{
            .name = forms.column(0).as_string(),
            .table = forms.column(1).as_string(),
            .default_form = forms.column(3).as_int() != 0,
            .presentation = {.name = forms.column(1).as_string(), .label = optional_string(forms.column(2))}};
        auto fields = database.prepare(
            "SELECT field_name, label, position, visible, read_only, format, currency_code, time_zone, "
            "lookup_display_field, lookup_search_fields, lookup_result_limit, lookup_allow_drill_down "
            "FROM _app_form_fields WHERE form_name = ? ORDER BY position, field_name");
        fields.bind(1, db::Value{form.name});
        while (fields.step()) {
            FieldMetadata field{.name = fields.column(0).as_string(),
                                .label = optional_string(fields.column(1)),
                                .order = optional_size(fields.column(2)),
                                .visible = optional_bool(fields.column(3)),
                                .read_only = optional_bool(fields.column(4)),
                                .format = parse_format(fields.column(5).as_string()),
                                .currency_code = optional_string(fields.column(6)),
                                .time_zone = optional_string(fields.column(7))};
            const auto display_field = optional_string(fields.column(8));
            const auto search_fields = parse_search_fields(fields.column(9));
            const auto result_limit = optional_size(fields.column(10));
            const auto allow_drill_down = optional_bool(fields.column(11));
            if (display_field || !search_fields.empty() || result_limit || allow_drill_down) {
                field.lookup = LookupMetadata{.display_field = display_field,
                                              .search_fields = search_fields,
                                              .result_limit = result_limit.value_or(100),
                                              .allow_drill_down = allow_drill_down.value_or(true)};
            }
            form.presentation.fields.push_back(std::move(field));
        }
        if (form.default_form)
            definition.presentation.tables.push_back(form.presentation);
        definition.forms.push_back(std::move(form));
    }
}

void load_version_two(db::Database& database, ApplicationDefinition& definition) {
    auto views = database.prepare(
        "SELECT name, table_name, label, form_name, default_filter_name FROM _app_views ORDER BY name");
    while (views.step()) {
        ViewDefinition view{.name = views.column(0).as_string(),
                            .table = views.column(1).as_string(),
                            .label = optional_string(views.column(2)),
                            .form = optional_string(views.column(3)),
                            .default_filter = optional_string(views.column(4))};
        auto filters = database.prepare(
            "SELECT name, search_text FROM _app_view_filters WHERE view_name = ? ORDER BY position, name");
        filters.bind(1, db::Value{view.name});
        while (filters.step()) {
            model::SavedFilter filter{.name = filters.column(0).as_string(),
                                      .search = optional_string(filters.column(1))};
            auto terms = database.prepare(
                "SELECT field_name, comparison, value_kind, integer_value, real_value, text_value, blob_value "
                "FROM _app_view_filter_terms WHERE view_name = ? AND filter_name = ? ORDER BY position");
            terms.bind(1, db::Value{view.name});
            terms.bind(2, db::Value{filter.name});
            while (terms.step()) {
                filter.filters.push_back({.field = terms.column(0).as_string(),
                                          .comparison = parse_filter_operator(terms.column(1).as_string()),
                                          .value = parse_filter_value(terms.column(2), terms.column(3), terms.column(4),
                                                                      terms.column(5), terms.column(6))});
            }
            view.filters.push_back(std::move(filter));
        }
        definition.views.push_back(std::move(view));
    }

    auto commands = database.prepare("SELECT name, label, command FROM _app_commands ORDER BY name");
    while (commands.step())
        definition.commands.push_back({.name = commands.column(0).as_string(),
                                       .label = optional_string(commands.column(1)),
                                       .command = commands.column(2).as_string()});

    auto reports = database.prepare("SELECT name, label, sql, row_limit FROM _app_reports ORDER BY name");
    while (reports.step())
        definition.reports.push_back({.name = reports.column(0).as_string(),
                                      .label = reports.column(1).as_string(),
                                      .sql = reports.column(2).as_string(),
                                      .row_limit = static_cast<std::size_t>(reports.column(3).as_int())});

    auto menus = database.prepare("SELECT name, label, position FROM _app_menus ORDER BY position, name");
    while (menus.step()) {
        MenuDefinition menu{.name = menus.column(0).as_string(),
                            .label = menus.column(1).as_string(),
                            .position = static_cast<std::size_t>(menus.column(2).as_int())};
        auto items =
            database.prepare("SELECT label, command_name FROM _app_menu_items WHERE menu_name = ? ORDER BY position");
        items.bind(1, db::Value{menu.name});
        while (items.step())
            menu.items.push_back({.label = items.column(0).as_string(), .command = items.column(1).as_string()});
        definition.menus.push_back(std::move(menu));
    }
}

void load_version_three(db::Database& database, ApplicationDefinition& definition) {
    auto scripts = database.prepare(
        "SELECT name, hook, table_name, command_name, source, position FROM _app_scripts ORDER BY position, name");
    while (scripts.step()) {
        const auto hook = script::parse_hook(scripts.column(1).as_string());
        if (!hook)
            throw Error{ErrorCategory::metadata, "unknown application script hook"};
        definition.scripts.push_back({.name = scripts.column(0).as_string(),
                                      .hook = *hook,
                                      .table = optional_string(scripts.column(2)),
                                      .command = optional_string(scripts.column(3)),
                                      .source = scripts.column(4).as_string(),
                                      .position = static_cast<std::size_t>(scripts.column(5).as_int())});
    }
}

void load_version_four(db::Database& database, ApplicationDefinition& definition) {
    auto screens = database.prepare("SELECT name, label, description, is_default FROM _app_screens ORDER BY name");
    while (screens.step()) {
        ScreenDefinition screen{.name = screens.column(0).as_string(),
                                .label = screens.column(1).as_string(),
                                .description = optional_string(screens.column(2)),
                                .default_screen = screens.column(3).as_int() != 0};
        auto items = database.prepare(
            "SELECT label, description, command_name FROM _app_screen_items WHERE screen_name = ? ORDER BY position");
        items.bind(1, db::Value{screen.name});
        while (items.step())
            screen.items.push_back({.label = items.column(0).as_string(),
                                    .description = optional_string(items.column(1)),
                                    .command = items.column(2).as_string()});
        definition.screens.push_back(std::move(screen));
    }
}

} // namespace

auto application_schema_version(db::Database& database) -> std::optional<int> {
    if (!table_exists(database, "_app_schema"))
        return std::nullopt;
    auto statement = database.prepare("SELECT version FROM _app_schema WHERE singleton = 1");
    if (!statement.step())
        throw Error{ErrorCategory::metadata, "application metadata schema has no version row"};
    const auto version = statement.column(0).as_int();
    if (statement.step())
        throw Error{ErrorCategory::metadata, "application metadata schema has multiple version rows"};
    if (version <= 0 || version > current_application_schema_version)
        throw Error{ErrorCategory::metadata,
                    "unsupported application metadata schema version: " + std::to_string(version)};
    return static_cast<int>(version);
}

auto has_application_metadata(db::Database& database) -> bool {
    return application_schema_version(database).has_value();
}

auto is_application_metadata_table(std::string_view table) noexcept -> bool {
    return table.starts_with("_app_");
}

void migrate_application_metadata(db::Database& database) {
    auto version = application_schema_version(database).value_or(0);
    if (version == current_application_schema_version)
        return;

    db::Transaction transaction{database};
    if (version == 0) {
        create_version_one(database);
        version = 1;
    }
    if (version == 1) {
        migrate_one_to_two(database);
        version = 2;
    }
    if (version == 2) {
        migrate_two_to_three(database);
        version = 3;
    }
    if (version == 3) {
        migrate_three_to_four(database);
        version = 4;
    }
    if (version == 4) {
        migrate_four_to_five(database);
        version = 5;
    }
    if (version != current_application_schema_version)
        throw Error{ErrorCategory::metadata, "no application metadata migration path is available"};
    transaction.commit();
}

auto load_application_definition(db::Database& database) -> ApplicationDefinition {
    const auto version = application_schema_version(database);
    if (!version)
        return {};
    if (*version != current_application_schema_version)
        throw Error{ErrorCategory::metadata, "application metadata must be upgraded before it can be loaded"};

    ApplicationDefinition definition;
    definition.schema_version = *version;
    load_settings(database, definition);
    load_forms(database, definition);
    load_version_two(database, definition);
    load_version_three(database, definition);
    load_version_four(database, definition);
    definition.validate(db::inspect_schema(database));
    return definition;
}

} // namespace vulpes::appmeta
