#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace vulpes::script {

enum class Hook { on_open, before_insert, before_update, after_update, before_delete, on_command };

struct Definition {
    std::string name;
    Hook hook{Hook::on_open};
    std::optional<std::string> table;
    std::optional<std::string> command;
    std::string source;
    std::size_t position{};
};

[[nodiscard]] auto hook_name(Hook hook) noexcept -> std::string_view;
[[nodiscard]] auto parse_hook(std::string_view name) noexcept -> std::optional<Hook>;

} // namespace vulpes::script
