#include "vulpes/script/hook.hpp"

namespace vulpes::script {

auto hook_name(Hook hook) noexcept -> std::string_view {
    switch (hook) {
    case Hook::on_open:
        return "on_open";
    case Hook::before_insert:
        return "before_insert";
    case Hook::before_update:
        return "before_update";
    case Hook::after_update:
        return "after_update";
    case Hook::before_delete:
        return "before_delete";
    case Hook::on_command:
        return "on_command";
    }
    return {};
}

auto parse_hook(std::string_view name) noexcept -> std::optional<Hook> {
    if (name == "on_open")
        return Hook::on_open;
    if (name == "before_insert")
        return Hook::before_insert;
    if (name == "before_update")
        return Hook::before_update;
    if (name == "after_update")
        return Hook::after_update;
    if (name == "before_delete")
        return Hook::before_delete;
    if (name == "on_command")
        return Hook::on_command;
    return std::nullopt;
}

} // namespace vulpes::script
