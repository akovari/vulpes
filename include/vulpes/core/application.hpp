#pragma once

#include "vulpes/core/command.hpp"
#include "vulpes/db/schema.hpp"

#include <optional>
#include <vector>

namespace vulpes::db {
class Database;
}

namespace vulpes::core {

enum class CommandOutcome { help, tables, schema, browse, quit, unknown_command, invalid_arguments, table_not_found };

// A semantic result deliberately contains no terminal coordinates or rendered
// text. Frontends localize and render the result for their own presentation.
struct CommandResponse {
    CommandOutcome outcome{CommandOutcome::help};
    CommandId command{CommandId::none};
    std::vector<db::TableSchema> tables;
    std::optional<db::TableSchema> table;
};

class ApplicationRuntime {
  public:
    explicit ApplicationRuntime(db::Database& database) : database_{&database} {}

    [[nodiscard]] auto execute(const Command& command) const -> CommandResponse;

  private:
    db::Database* database_;
};

} // namespace vulpes::core
