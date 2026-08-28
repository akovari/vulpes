#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace vulpes::terminal {

[[nodiscard]] auto cell_width(char32_t code_point) noexcept -> int;
[[nodiscard]] auto encode_utf8(char32_t code_point) -> std::string;
[[nodiscard]] auto first_code_point(std::string_view text) noexcept -> char32_t;
[[nodiscard]] auto lowercase_code_point(char32_t code_point) noexcept -> char32_t;
[[nodiscard]] auto find_code_point_column(std::string_view text, char32_t code_point) -> std::optional<int>;
[[nodiscard]] auto text_width(std::string_view text) -> int;
[[nodiscard]] auto truncate_utf8(std::string_view text, int maximum_width) -> std::string;

} // namespace vulpes::terminal
