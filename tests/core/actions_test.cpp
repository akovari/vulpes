#include "vulpes/core/actions.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("action map converts terminal keys to stable semantic actions", "[core][actions]") {
    vulpes::core::ActionMap actions;
    CHECK(actions.action_for(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f2}) ==
          vulpes::core::ActionId::record_edit);
    CHECK(actions.action_for(
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'p', .ctrl = true}) ==
          vulpes::core::ActionId::application_command_palette);
    CHECK(actions.action_for(
              vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::character, .character = U'r', .ctrl = true}) ==
          vulpes::core::ActionId::database_open_read_only);
    CHECK(vulpes::core::action_id(vulpes::core::ActionId::application_command_palette) ==
          "application.command_palette");
    CHECK(vulpes::core::action_id(vulpes::core::ActionId::dataset_filter) == "dataset.filter");

    actions.bind({.key = {.key = vulpes::terminal::Key::f7}, .action = vulpes::core::ActionId::record_edit});
    CHECK(actions.action_for(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f7}) ==
          vulpes::core::ActionId::record_edit);
    CHECK(actions.action_for(vulpes::terminal::KeyEvent{.key = vulpes::terminal::Key::f8}) ==
          vulpes::core::ActionId::none);
}
