#include "vulpes/db/row.hpp"

#include <stdexcept>

namespace vulpes::db {

Row::Row(std::vector<std::string> names, std::vector<Value> values)
    : names_{std::move(names)}, values_{std::move(values)} {
    if (names_.size() != values_.size()) {
        throw std::invalid_argument{"row names and values must have the same size"};
    }
}

auto Row::column_name(std::size_t index) const -> std::string_view {
    return names_.at(index);
}
auto Row::at(std::size_t index) const -> const Value& {
    return values_.at(index);
}

auto Row::at(std::string_view name) const -> const Value& {
    for (std::size_t index = 0; index < names_.size(); ++index) {
        if (names_[index] == name)
            return values_[index];
    }
    throw std::out_of_range{"row has no column named '" + std::string{name} + "'"};
}

} // namespace vulpes::db
