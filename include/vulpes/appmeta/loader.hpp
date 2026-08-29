#pragma once

#include "vulpes/appmeta/definition.hpp"

#include <optional>
#include <string_view>

namespace vulpes::db {
class Database;
}

namespace vulpes::appmeta {

inline constexpr int current_application_schema_version = 3;

[[nodiscard]] auto application_schema_version(db::Database& database) -> std::optional<int>;
[[nodiscard]] auto has_application_metadata(db::Database& database) -> bool;
[[nodiscard]] auto is_application_metadata_table(std::string_view table) noexcept -> bool;

// Migration is explicit and transactional. Loading an ordinary database never
// creates or modifies application metadata tables.
void migrate_application_metadata(db::Database& database);
[[nodiscard]] auto load_application_definition(db::Database& database) -> ApplicationDefinition;

} // namespace vulpes::appmeta
