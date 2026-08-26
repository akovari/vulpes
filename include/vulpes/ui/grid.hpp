#pragma once

#include "vulpes/model/dataset.hpp"
#include "vulpes/terminal/screen_buffer.hpp"

#include <string>

namespace vulpes::ui {

struct Rect {
    int x{};
    int y{};
    int width{};
    int height{};
};

class Grid {
public:
    Grid(const model::Dataset& dataset, std::string title);
    void render(terminal::ScreenBuffer& buffer, Rect bounds) const;

private:
    const model::Dataset* dataset_;
    std::string title_;
};

} // namespace vulpes::ui

