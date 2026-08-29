#pragma once

#include <cstddef>
#include <span>

namespace vulpes::report::detail {

[[nodiscard]] auto default_pdf_font() noexcept -> std::span<const std::byte>;

} // namespace vulpes::report::detail
