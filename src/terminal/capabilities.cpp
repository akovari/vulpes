#include "vulpes/terminal/capabilities.hpp"

#include <cstdio>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace vulpes::terminal {
namespace {

[[nodiscard]] auto stream_is_terminal(std::FILE* stream) noexcept -> bool {
#ifdef _WIN32
    const int descriptor = _fileno(stream);
    return descriptor >= 0 && _isatty(descriptor) != 0;
#else
    const int descriptor = fileno(stream);
    return descriptor >= 0 && isatty(descriptor) != 0;
#endif
}

} // namespace

auto detect_console_capabilities() noexcept -> ConsoleCapabilities {
    return {
        .standard_input_is_terminal = stream_is_terminal(stdin),
        .standard_output_is_terminal = stream_is_terminal(stdout),
    };
}

auto console_capability_issue(ConsoleCapabilities capabilities) noexcept -> ConsoleCapabilityIssue {
    if (!capabilities.standard_input_is_terminal && !capabilities.standard_output_is_terminal)
        return ConsoleCapabilityIssue::input_and_output_not_terminal;
    if (!capabilities.standard_input_is_terminal)
        return ConsoleCapabilityIssue::input_not_terminal;
    if (!capabilities.standard_output_is_terminal)
        return ConsoleCapabilityIssue::output_not_terminal;
    return ConsoleCapabilityIssue::none;
}

auto console_capability_message(ConsoleCapabilityIssue issue) noexcept -> std::string_view {
    switch (issue) {
    case ConsoleCapabilityIssue::input_not_terminal:
        return "interactive terminal requires terminal-connected standard input; input is redirected or unavailable";
    case ConsoleCapabilityIssue::output_not_terminal:
        return "interactive terminal requires terminal-connected standard output; output is redirected or unavailable";
    case ConsoleCapabilityIssue::input_and_output_not_terminal:
        return "interactive terminal requires terminal-connected standard streams; input and output are redirected or "
               "unavailable";
    case ConsoleCapabilityIssue::none:
        return {};
    }
    return {};
}

} // namespace vulpes::terminal
