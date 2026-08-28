#pragma once

#include "vulpes/appmeta/metadata.hpp"

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

struct FormDefinition {
    std::string name;
    std::string table;
    bool default_form{false};
    TableMetadata presentation;
};

struct ViewDefinition {
    std::string name;
    std::string table;
    std::optional<std::string> label;
    std::optional<std::string> form;
};

struct CommandDefinition {
    std::string name;
    std::optional<std::string> label;
    std::string command;
};

struct MenuItemDefinition {
    std::string label;
    std::string command;
};

struct MenuDefinition {
    std::string name;
    std::string label;
    std::size_t position{};
    std::vector<MenuItemDefinition> items;
};

struct ReportDefinition {
    std::string name;
    std::string label;
    std::string sql;
    std::size_t row_limit{1'000};
};

struct SettingDefinition {
    std::string key;
    std::string value;
};

// A loaded application definition owns semantic metadata only. It remains
// independent of terminal layout and can be consumed by future frontends.
class ApplicationDefinition {
  public:
    int schema_version{};
    ApplicationMetadata presentation;
    std::vector<FormDefinition> forms;
    std::vector<ViewDefinition> views;
    std::vector<CommandDefinition> commands;
    std::vector<MenuDefinition> menus;
    std::vector<ReportDefinition> reports;
    std::vector<SettingDefinition> settings;

    [[nodiscard]] auto empty() const noexcept -> bool { return schema_version == 0; }
    [[nodiscard]] auto form(std::string_view name) const noexcept -> const FormDefinition*;
    [[nodiscard]] auto default_form(std::string_view table) const noexcept -> const FormDefinition*;
    [[nodiscard]] auto view(std::string_view name) const noexcept -> const ViewDefinition*;
    [[nodiscard]] auto command(std::string_view name) const noexcept -> const CommandDefinition*;
    [[nodiscard]] auto report(std::string_view name) const noexcept -> const ReportDefinition*;
    [[nodiscard]] auto setting(std::string_view key) const noexcept -> std::optional<std::string_view>;
    void validate(std::span<const db::TableSchema> schema) const;
};

} // namespace vulpes::appmeta
