#pragma once

#include <string>
#include <string_view>

namespace vulpes::db::detail {

// Quotes one SQLite identifier. It is intentionally separate from value binding:
// identifiers cannot be SQL parameters and must come from trusted schema metadata.
[[nodiscard]] auto quote_identifier(std::string_view identifier) -> std::string;

} // namespace vulpes::db::detail
