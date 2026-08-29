#include "vulpes/core/error.hpp"
#include "vulpes/model/lifecycle.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dataset lifecycle records preserve field ownership and write policy", "[model][lifecycle]") {
    vulpes::model::DatasetRecord record{"customer",
                                        {{.name = "name", .value = vulpes::db::Value{"Ada"}, .writable = true},
                                         {.name = "id", .value = vulpes::db::Value{1}, .writable = false}}};

    REQUIRE(record.field("name") != nullptr);
    CHECK(record.field("name")->value->as_string() == "Ada");
    record.set("name", vulpes::db::Value{"Grace"});
    CHECK(record.field("name")->value->as_string() == "Grace");
    CHECK_THROWS_AS(record.set("id", vulpes::db::Value{2}), vulpes::Error);
    CHECK_THROWS_AS(record.set("missing", vulpes::db::Value{"value"}), vulpes::Error);
    CHECK(vulpes::model::dataset_hook_name(vulpes::model::DatasetHook::before_insert) == "before_insert");
}
