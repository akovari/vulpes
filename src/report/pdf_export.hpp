#pragma once

#include "vulpes/core/formatting.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/value.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace vulpes::report::detail {

[[nodiscard]] auto display_text(const db::Value& value, const core::LocaleFormatter& formatter) -> std::string;

void write_pdf(const std::filesystem::path& path, const db::SqlResult& result, std::string_view title,
               const core::LocaleFormatter& formatter);

} // namespace vulpes::report::detail
