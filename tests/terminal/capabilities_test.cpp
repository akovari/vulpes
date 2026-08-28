#include "vulpes/terminal/capabilities.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("console capability validation distinguishes redirected streams", "[terminal][capabilities]") {
    using vulpes::terminal::console_capability_issue;
    using vulpes::terminal::console_capability_message;
    using vulpes::terminal::ConsoleCapabilities;
    using vulpes::terminal::ConsoleCapabilityIssue;

    const ConsoleCapabilities interactive{.standard_input_is_terminal = true, .standard_output_is_terminal = true};
    CHECK(interactive.supports_interactive_terminal());
    CHECK(console_capability_issue(interactive) == ConsoleCapabilityIssue::none);
    CHECK(console_capability_message(ConsoleCapabilityIssue::none).empty());

    const ConsoleCapabilities input_redirected{.standard_output_is_terminal = true};
    CHECK_FALSE(input_redirected.supports_interactive_terminal());
    CHECK(console_capability_issue(input_redirected) == ConsoleCapabilityIssue::input_not_terminal);

    const ConsoleCapabilities output_redirected{.standard_input_is_terminal = true};
    CHECK(console_capability_issue(output_redirected) == ConsoleCapabilityIssue::output_not_terminal);

    const ConsoleCapabilities both_redirected{};
    CHECK(console_capability_issue(both_redirected) == ConsoleCapabilityIssue::input_and_output_not_terminal);
    CHECK_FALSE(console_capability_message(ConsoleCapabilityIssue::input_and_output_not_terminal).empty());
}
