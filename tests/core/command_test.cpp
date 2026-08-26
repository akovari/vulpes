#include "vulpes/core/command.hpp"
#include "vulpes/core/error.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace vulpes::core;

TEST_CASE("commands have stable semantic IDs and preserve UTF-8 arguments", "[core][command]") {
    const auto command = parse_command("  BROWSE \"zákazníci 2026\"  ");
    CHECK(command.id == CommandId::browse);
    REQUIRE(command.arguments.size() == 1);
    CHECK(command.arguments.front() == "zákazníci 2026");
    CHECK(action_id(command.id) == "dataset.browse");
    CHECK(parse_command("sql").id == CommandId::sql);
    CHECK(action_id(CommandId::sql) == "database.sql");
}

TEST_CASE("command parser handles aliases and invalid quotation", "[core][command]") {
    CHECK(parse_command("?").id == CommandId::help);
    CHECK(parse_command("exit").id == CommandId::quit);
    CHECK(parse_command("invented").id == CommandId::unknown);
    CHECK_THROWS_AS(parse_command("browse \"customers"), vulpes::Error);
}
