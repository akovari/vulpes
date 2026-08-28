#include "vulpes/core/command.hpp"

#include "vulpes/core/error.hpp"

#include <cctype>
#include <iterator>
#include <utility>

namespace vulpes::core {
namespace {

auto lowercase_ascii(std::string text) -> std::string {
    for (auto& character : text) {
        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return text;
}

auto command_id(std::string_view verb) -> CommandId {
    if (verb == "help" || verb == "?")
        return CommandId::help;
    if (verb == "tables")
        return CommandId::tables;
    if (verb == "schema")
        return CommandId::schema;
    if (verb == "browse")
        return CommandId::browse;
    if (verb == "forms")
        return CommandId::forms;
    if (verb == "form")
        return CommandId::form;
    if (verb == "views")
        return CommandId::views;
    if (verb == "view")
        return CommandId::view;
    if (verb == "reports")
        return CommandId::reports;
    if (verb == "report")
        return CommandId::report;
    if (verb == "run")
        return CommandId::run;
    if (verb == "sql")
        return CommandId::sql;
    if (verb == "quit" || verb == "exit")
        return CommandId::quit;
    return CommandId::unknown;
}

} // namespace

auto parse_command(std::string_view source) -> Command {
    std::vector<std::string> tokens;
    std::size_t position = 0;
    while (position < source.size()) {
        while (position < source.size() && std::isspace(static_cast<unsigned char>(source[position])))
            ++position;
        if (position == source.size())
            break;

        std::string token;
        if (source[position] == '"') {
            ++position;
            bool closed = false;
            while (position < source.size()) {
                const char character = source[position++];
                if (character == '"') {
                    closed = true;
                    break;
                }
                if (character == '\\' && position < source.size())
                    token += source[position++];
                else
                    token += character;
            }
            if (!closed)
                throw Error{ErrorCategory::validation, "unterminated quoted command argument"};
        } else {
            while (position < source.size() && !std::isspace(static_cast<unsigned char>(source[position])))
                token += source[position++];
        }
        tokens.push_back(std::move(token));
    }

    if (tokens.empty())
        return {};
    Command command;
    command.name = lowercase_ascii(std::move(tokens.front()));
    command.id = command_id(command.name);
    command.arguments.assign(std::next(tokens.begin()), tokens.end());
    return command;
}

auto action_id(CommandId command) -> std::string_view {
    switch (command) {
    case CommandId::help:
        return "application.help";
    case CommandId::tables:
        return "database.tables";
    case CommandId::schema:
        return "database.schema";
    case CommandId::browse:
        return "dataset.browse";
    case CommandId::forms:
        return "application.forms";
    case CommandId::form:
        return "application.form";
    case CommandId::views:
        return "application.views";
    case CommandId::view:
        return "application.view";
    case CommandId::reports:
        return "application.reports";
    case CommandId::report:
        return "application.report";
    case CommandId::run:
        return "application.run";
    case CommandId::sql:
        return "database.sql";
    case CommandId::quit:
        return "application.quit";
    case CommandId::none:
        return {};
    case CommandId::unknown:
        return "application.unknown";
    }
    return {};
}

} // namespace vulpes::core
