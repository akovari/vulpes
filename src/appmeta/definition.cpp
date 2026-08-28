#include "vulpes/appmeta/loader.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/db/schema.hpp"

#include <algorithm>
#include <set>

namespace vulpes::appmeta {
namespace {

template <typename Item>
auto find_named(const std::vector<Item>& items, std::string_view name) noexcept -> const Item* {
    const auto found = std::ranges::find(items, name, &Item::name);
    return found == items.end() ? nullptr : &*found;
}

template <typename Item> void require_unique_names(const std::vector<Item>& items, std::string_view kind) {
    std::set<std::string, std::less<>> names;
    for (const auto& item : items) {
        if (item.name.empty() || !names.insert(item.name).second)
            throw Error{ErrorCategory::metadata,
                        "application definition contains an empty or duplicate " + std::string{kind} + " name"};
    }
}

[[nodiscard]] auto has_table(std::span<const db::TableSchema> schema, std::string_view name) -> bool {
    return std::ranges::find(schema, name, &db::TableSchema::name) != schema.end();
}

} // namespace

auto ApplicationDefinition::form(std::string_view name) const noexcept -> const FormDefinition* {
    return find_named(forms, name);
}

auto ApplicationDefinition::default_form(std::string_view table) const noexcept -> const FormDefinition* {
    const auto found = std::ranges::find_if(forms, [&](const auto& form_definition) {
        return form_definition.table == table && form_definition.default_form;
    });
    return found == forms.end() ? nullptr : &*found;
}

auto ApplicationDefinition::view(std::string_view name) const noexcept -> const ViewDefinition* {
    return find_named(views, name);
}

auto ApplicationDefinition::command(std::string_view name) const noexcept -> const CommandDefinition* {
    return find_named(commands, name);
}

auto ApplicationDefinition::report(std::string_view name) const noexcept -> const ReportDefinition* {
    return find_named(reports, name);
}

auto ApplicationDefinition::setting(std::string_view key) const noexcept -> std::optional<std::string_view> {
    const auto found = std::ranges::find(settings, key, &SettingDefinition::key);
    return found == settings.end() ? std::nullopt : std::optional<std::string_view>{found->value};
}

void ApplicationDefinition::validate(std::span<const db::TableSchema> schema) const {
    if (schema_version != current_application_schema_version)
        throw Error{ErrorCategory::metadata, "application definition has an unsupported schema version"};

    presentation.validate(schema);
    require_unique_names(forms, "form");
    require_unique_names(views, "view");
    require_unique_names(commands, "command");
    require_unique_names(menus, "menu");
    require_unique_names(reports, "report");

    std::set<std::string, std::less<>> default_tables;
    for (const auto& form_definition : forms) {
        if (!has_table(schema, form_definition.table) || is_application_metadata_table(form_definition.table))
            throw Error{ErrorCategory::metadata,
                        "form references an unknown application table: " + form_definition.name};
        if (form_definition.presentation.name != form_definition.table)
            throw Error{ErrorCategory::metadata, "form presentation does not match its table: " + form_definition.name};
        ApplicationMetadata{{form_definition.presentation}}.validate(schema);
        if (form_definition.default_form && !default_tables.insert(form_definition.table).second)
            throw Error{ErrorCategory::metadata, "more than one default form targets table: " + form_definition.table};
    }

    for (const auto& view_definition : views) {
        if (!has_table(schema, view_definition.table) || is_application_metadata_table(view_definition.table))
            throw Error{ErrorCategory::metadata,
                        "view references an unknown application table: " + view_definition.name};
        if (view_definition.label && view_definition.label->empty())
            throw Error{ErrorCategory::metadata, "view label cannot be empty: " + view_definition.name};
        if (view_definition.form) {
            const auto* form_definition = form(*view_definition.form);
            if (form_definition == nullptr || form_definition->table != view_definition.table)
                throw Error{ErrorCategory::metadata, "view references an incompatible form: " + view_definition.name};
        }
    }

    std::set<std::string, std::less<>> setting_keys;
    for (const auto& setting_definition : settings) {
        if (setting_definition.key.empty() || !setting_keys.insert(setting_definition.key).second)
            throw Error{ErrorCategory::metadata, "application definition contains an empty or duplicate setting key"};
    }
    for (const auto& command_definition : commands) {
        if ((command_definition.label && command_definition.label->empty()) || command_definition.command.empty())
            throw Error{ErrorCategory::metadata,
                        "command requires a non-empty label and command: " + command_definition.name};
    }
    for (const auto& report_definition : reports) {
        if (report_definition.label.empty() || report_definition.sql.empty() || report_definition.row_limit == 0 ||
            report_definition.row_limit > 100'000) {
            throw Error{ErrorCategory::metadata,
                        "report has an invalid label, query, or row limit: " + report_definition.name};
        }
    }
    for (const auto& menu : menus) {
        if (menu.label.empty())
            throw Error{ErrorCategory::metadata, "menu label cannot be empty: " + menu.name};
        for (const auto& item : menu.items) {
            if (item.label.empty() || command(item.command) == nullptr)
                throw Error{ErrorCategory::metadata, "menu item references an unknown command: " + menu.name};
        }
    }
}

} // namespace vulpes::appmeta
