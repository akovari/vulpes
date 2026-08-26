#pragma once

#include "vulpes/db/value.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vulpes::db {

// An owning database row. It deliberately outlives the Statement that produced it.
class Row {
  public:
    Row() = default;
    Row(std::vector<std::string> names, std::vector<Value> values);

    [[nodiscard]] auto size() const noexcept -> std::size_t { return values_.size(); }
    [[nodiscard]] auto column_name(std::size_t index) const -> std::string_view;
    [[nodiscard]] auto at(std::size_t index) const -> const Value&;
    [[nodiscard]] auto at(std::string_view name) const -> const Value&;

  private:
    std::vector<std::string> names_;
    std::vector<Value> values_;
};

} // namespace vulpes::db
