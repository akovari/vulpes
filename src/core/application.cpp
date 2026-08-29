#include "vulpes/core/application.hpp"

#include "vulpes/appmeta/loader.hpp"
#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/script/runtime.hpp"

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
    if (scripts_ != nullptr && command.id != CommandId::none)
        scripts_->on_command(command.name);
    return execute(command, 0);
}

auto ApplicationRuntime::execute(const Command& command, std::size_t depth) const -> CommandResponse {
    if (depth > 16)
        return {.outcome = CommandOutcome::command_cycle, .command = command.id};
    switch (command.id) {
    case CommandId::none:
    case CommandId::help:
        return requires_no_arguments(command, CommandOutcome::help);
    case CommandId::quit:
        return requires_no_arguments(command, CommandOutcome::quit);
    case CommandId::sql:
        return requires_no_arguments(command, CommandOutcome::sql);
    case CommandId::unknown:
        if (definition_ != nullptr) {
            if (const auto* definition = definition_->command(command.name); definition != nullptr) {
                if (!command.arguments.empty())
                    return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
                return execute(parse_command(definition->command), depth + 1);
            }
        }
        return {.outcome = CommandOutcome::unknown_command, .command = command.id};
    case CommandId::tables: {
        auto response = requires_no_arguments(command, CommandOutcome::tables);
        if (response.outcome == CommandOutcome::tables)
            response.tables = db::inspect_schema(*database_);
        if (definition_ != nullptr && !definition_->empty()) {
            std::erase_if(response.tables,
                          [](const auto& table) { return appmeta::is_application_metadata_table(table.name); });
        }
        return response;
    }
    case CommandId::forms: {
        auto response = requires_no_arguments(command, CommandOutcome::forms);
        if (response.outcome == CommandOutcome::forms && definition_ != nullptr)
            response.forms = definition_->forms;
        return response;
    }
    case CommandId::views: {
        auto response = requires_no_arguments(command, CommandOutcome::views);
        if (response.outcome == CommandOutcome::views && definition_ != nullptr)
            response.views = definition_->views;
        return response;
    }
    case CommandId::reports: {
        auto response = requires_no_arguments(command, CommandOutcome::reports);
        if (response.outcome == CommandOutcome::reports && definition_ != nullptr)
            response.reports = definition_->reports;
        return response;
    }
    case CommandId::run:
        if (command.arguments.size() != 1)
            return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
        if (definition_ == nullptr)
            return {.outcome = CommandOutcome::definition_not_found, .command = command.id};
        if (const auto* definition = definition_->command(command.arguments.front()); definition != nullptr)
            return execute(parse_command(definition->command), depth + 1);
        return {.outcome = CommandOutcome::definition_not_found, .command = command.id};
    case CommandId::report:
        if (command.arguments.size() != 1)
            return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
        if (definition_ != nullptr) {
            if (const auto* report = definition_->report(command.arguments.front()); report != nullptr)
                return {.outcome = CommandOutcome::report, .command = command.id, .report = *report};
        }
        return {.outcome = CommandOutcome::definition_not_found, .command = command.id};
    case CommandId::export_report:
        if (command.arguments.size() != 3 && command.arguments.size() != 4)
            return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
        if (command.arguments.size() == 4 && command.arguments[3] != "overwrite")
            return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
        if (definition_ != nullptr) {
            if (const auto* report_definition = definition_->report(command.arguments[0]);
                report_definition != nullptr) {
                try {
                    return {.outcome = CommandOutcome::export_report,
                            .command = command.id,
                            .report = *report_definition,
                            .export_format = report::parse_export_format(command.arguments[1]),
                            .export_destination = command.arguments[2],
                            .export_overwrite = command.arguments.size() == 4};
                } catch (const Error&) {
                    return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
                }
            }
        }
        return {.outcome = CommandOutcome::definition_not_found, .command = command.id};
    case CommandId::schema:
    case CommandId::browse:
        break;
    case CommandId::form:
    case CommandId::view:
        if (command.arguments.size() != 1)
            return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};
        if (definition_ == nullptr)
            return {.outcome = CommandOutcome::definition_not_found, .command = command.id};
        break;
    }

    if (command.arguments.size() != 1)
        return {.outcome = CommandOutcome::invalid_arguments, .command = command.id};

    if (command.id == CommandId::form) {
        const auto* form = definition_->form(command.arguments.front());
        if (form == nullptr)
            return {.outcome = CommandOutcome::definition_not_found, .command = command.id};
        auto tables = db::inspect_schema(*database_);
        const auto table = std::ranges::find(tables, form->table, &db::TableSchema::name);
        if (table == tables.end())
            return {.outcome = CommandOutcome::table_not_found, .command = command.id};
        return {.outcome = CommandOutcome::browse, .command = command.id, .table = *table, .form = *form};
    }
    if (command.id == CommandId::view) {
        const auto* view = definition_->view(command.arguments.front());
        if (view == nullptr)
            return {.outcome = CommandOutcome::definition_not_found, .command = command.id};
        auto tables = db::inspect_schema(*database_);
        const auto table = std::ranges::find(tables, view->table, &db::TableSchema::name);
        if (table == tables.end())
            return {.outcome = CommandOutcome::table_not_found, .command = command.id};
        CommandResponse response{
            .outcome = CommandOutcome::browse, .command = command.id, .table = *table, .view = *view};
        if (view->form)
            response.form = *definition_->form(*view->form);
        return response;
    }

    auto tables = db::inspect_schema(*database_);
    const auto table = std::ranges::find(tables, command.arguments.front(), &db::TableSchema::name);
    if (table == tables.end() ||
        (definition_ != nullptr && !definition_->empty() && appmeta::is_application_metadata_table(table->name)))
        return {.outcome = CommandOutcome::table_not_found, .command = command.id};

    return {.outcome = command.id == CommandId::schema ? CommandOutcome::schema : CommandOutcome::browse,
            .command = command.id,
            .table = *table};
}

} // namespace vulpes::core
