#pragma once

#include "vulpes/appmeta/definition.hpp"
#include "vulpes/core/command.hpp"
#include "vulpes/db/schema.hpp"

#include <optional>
#include <vector>

namespace vulpes::db {
class Database;
}

namespace vulpes::core {

enum class CommandOutcome {
    help,
    tables,
    schema,
    browse,
    sql,
    quit,
    forms,
    views,
    reports,
    report,
    unknown_command,
    invalid_arguments,
    table_not_found,
    definition_not_found,
    command_cycle
};

// A semantic result deliberately contains no terminal coordinates or rendered
// text. Frontends localize and render the result for their own presentation.
struct CommandResponse {
    CommandOutcome outcome{CommandOutcome::help};
    CommandId command{CommandId::none};
    std::vector<db::TableSchema> tables;
    std::optional<db::TableSchema> table;
    std::vector<appmeta::FormDefinition> forms;
    std::vector<appmeta::ViewDefinition> views;
    std::vector<appmeta::ReportDefinition> reports;
    std::optional<appmeta::FormDefinition> form;
    std::optional<appmeta::ViewDefinition> view;
    std::optional<appmeta::ReportDefinition> report;
};

class ApplicationRuntime {
  public:
    explicit ApplicationRuntime(db::Database& database, const appmeta::ApplicationDefinition* definition = nullptr)
        : database_{&database}, definition_{definition} {}

    [[nodiscard]] auto execute(const Command& command) const -> CommandResponse;

  private:
    [[nodiscard]] auto execute(const Command& command, std::size_t depth) const -> CommandResponse;

    db::Database* database_;
    const appmeta::ApplicationDefinition* definition_;
};

} // namespace vulpes::core
