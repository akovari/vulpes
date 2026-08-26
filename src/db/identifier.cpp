#include "vulpes/db/identifier.hpp"

namespace vulpes::db::detail {

auto quote_identifier(std::string_view identifier) -> std::string {
    std::string result{"\""};
    for (const char character : identifier) {
        result += character;
        if (character == '\"')
            result += '\"';
    }
    return result + '"';
}

} // namespace vulpes::db::detail
