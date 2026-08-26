#include "vulpes/core/application.hpp"

#include "vulpes/db/database.hpp"

#include <algorithm>

namespace vulpes::core {
namespace {

[[nodiscard]] auto requires_no_arguments(const Command& command, CommandOutcome outcome) -> CommandResponse {
    if (command.arguments.empty())
        return {.outcome = outcome, .command = command.id};
    return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
}

} // namespace

auto ApplicationRuntime::execute(const Command& command) const -> CommandResponse {
    switch (command.id) {
    case CommandId::none:
    case CommandId::help:
        return requires_no_arguments(command, CommandOutcome::help);
    case CommandId::quit:
        return requires_no_arguments(command, CommandOutcome::quit);
    case CommandId::unknown:
        return {.outcome = CommandOutcome::unknown_command, .command = command.id};
    case CommandId::tables: {
        auto response = requires_no_arguments(command, CommandOutcome::tables);
        if (response.outcome == CommandOutcome::tables)
            response.tables = db::inspect_schema(*database_);
        return response;
    }
    case CommandId::schema:
    case CommandId::browse:
        break;
    }

    if (command.arguments.size() != 1)
        return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};

    auto tables = db::inspect_schema(*database_);
    const auto table = std::ranges::find(tables, command.arguments.front(), &db::TableSchema::name);
    if (table == tables.end())
        return {.outcome = CommandOutcome::table_not_found, .command = command.id};

    return {.outcome = command.id == CommandId::schema ? CommandOutcome::schema : CommandOutcome::browse,
            .command = command.id,
            .table = *table};
}

} // namespace vulpes::core
