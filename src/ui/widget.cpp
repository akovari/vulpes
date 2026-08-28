#include "vulpes/ui/widget.hpp"

#include <algorithm>

namespace vulpes::ui {

Widget::~Widget() = default;

void Widget::layout(Rect bounds) {
    bounds.width = std::max(0, bounds.width);
    bounds.height = std::max(0, bounds.height);
    bounds_ = bounds;
}

} // namespace vulpes::ui
