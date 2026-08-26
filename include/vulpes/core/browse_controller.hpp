#pragma once

#include "vulpes/core/actions.hpp"
#include "vulpes/model/dataset.hpp"

namespace vulpes::core {

enum class BrowseResult { unchanged, redraw, close };

class BrowseController {
  public:
    explicit BrowseController(model::Dataset& dataset) : dataset_{&dataset} {}
    [[nodiscard]] auto handle(ActionId action) -> BrowseResult;

  private:
    model::Dataset* dataset_;
};

} // namespace vulpes::core
