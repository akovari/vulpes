#pragma once

#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/terminal.hpp"

namespace vulpes::core {

enum class BrowseResult { unchanged, redraw, close };

class BrowseController {
  public:
    explicit BrowseController(model::Dataset& dataset) : dataset_{&dataset} {}
    [[nodiscard]] auto handle(const terminal::InputEvent& event) -> BrowseResult;

  private:
    model::Dataset* dataset_;
};

} // namespace vulpes::core
