#pragma once

#include <string_view>

namespace vulpes::terminal {

struct ConsoleCapabilities {
    bool standard_input_is_terminal{};
    bool standard_output_is_terminal{};

    [[nodiscard]] auto supports_interactive_terminal() const noexcept -> bool {
        return standard_input_is_terminal && standard_output_is_terminal;
    }
};

enum class ConsoleCapabilityIssue { none, input_not_terminal, output_not_terminal, input_and_output_not_terminal };

[[nodiscard]] auto detect_console_capabilities() noexcept -> ConsoleCapabilities;
[[nodiscard]] auto console_capability_issue(ConsoleCapabilities capabilities) noexcept -> ConsoleCapabilityIssue;
[[nodiscard]] auto console_capability_message(ConsoleCapabilityIssue issue) noexcept -> std::string_view;

} // namespace vulpes::terminal
