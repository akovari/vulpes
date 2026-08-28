#include "vulpes/core/clipboard.hpp"

#include <clip.h>
#include <string>

namespace vulpes::core {

auto SystemClipboard::read_text() -> std::optional<std::string> {
    std::string text;
    if (!clip::get_text(text))
        return std::nullopt;
    return text;
}

auto SystemClipboard::write_text(std::string_view text) -> bool {
    return clip::set_text(std::string{text});
}

} // namespace vulpes::core
