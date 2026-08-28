#include "vulpes/terminal/unicode.hpp"

#include <algorithm>
#include <utf8proc.h>

namespace vulpes::terminal {

auto cell_width(char32_t code_point) noexcept -> int {
    return utf8proc_charwidth(static_cast<utf8proc_int32_t>(code_point));
}

auto encode_utf8(char32_t code_point) -> std::string {
    utf8proc_uint8_t encoded[4]{};
    const auto size = utf8proc_encode_char(static_cast<utf8proc_int32_t>(code_point), encoded);
    if (size < 0)
        return "\xEF\xBF\xBD";
    return {reinterpret_cast<const char*>(encoded), static_cast<std::size_t>(size)};
}

auto first_code_point(std::string_view text) noexcept -> char32_t {
    if (text.empty())
        return U'\0';
    utf8proc_int32_t code_point{};
    const auto size = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(text.data()),
                                       static_cast<utf8proc_ssize_t>(text.size()), &code_point);
    return size > 0 ? static_cast<char32_t>(code_point) : U'\0';
}

auto text_width(std::string_view text) -> int {
    const auto* cursor = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    auto remaining = static_cast<utf8proc_ssize_t>(text.size());
    int width = 0;
    while (remaining > 0) {
        utf8proc_int32_t code_point{};
        const auto consumed = utf8proc_iterate(cursor, remaining, &code_point);
        if (consumed < 0)
            return width;
        cursor += consumed;
        remaining -= consumed;
        width += std::max(0, cell_width(static_cast<char32_t>(code_point)));
    }
    return width;
}

auto truncate_utf8(std::string_view text, int maximum_width) -> std::string {
    if (maximum_width <= 0)
        return {};
    const auto* cursor = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    auto remaining = static_cast<utf8proc_ssize_t>(text.size());
    int width = 0;
    std::string result;
    while (remaining > 0) {
        utf8proc_int32_t code_point{};
        const auto consumed = utf8proc_iterate(cursor, remaining, &code_point);
        if (consumed < 0)
            break;
        const int code_point_width = std::max(0, cell_width(static_cast<char32_t>(code_point)));
        if (width + code_point_width > maximum_width)
            break;
        result.append(reinterpret_cast<const char*>(cursor), static_cast<std::size_t>(consumed));
        cursor += consumed;
        remaining -= consumed;
        width += code_point_width;
    }
    return result;
}

} // namespace vulpes::terminal
