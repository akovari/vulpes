#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace vulpes::core {

// Stable semantic IDs are intentionally distinct from localized command labels.
enum class CommandId {
    none,
    help,
    tables,
    schema,
    browse,
    forms,
    form,
    screens,
    screen,
    views,
    view,
    reports,
    report,
    export_report,
    run,
    sql,
    quit,
    unknown
};

struct Command {
    CommandId id{CommandId::none};
    std::string name;
    std::vector<std::string> arguments;
};

[[nodiscard]] auto parse_command(std::string_view source) -> Command;
[[nodiscard]] auto action_id(CommandId command) -> std::string_view;

} // namespace vulpes::core
