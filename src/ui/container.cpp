#include "vulpes/ui/container.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace vulpes::ui {
namespace {

[[nodiscard]] auto main_extent(Extent extent, Axis axis) noexcept -> int {
    return axis == Axis::horizontal ? extent.width : extent.height;
}

[[nodiscard]] auto cross_extent(Extent extent, Axis axis) noexcept -> int {
    return axis == Axis::horizontal ? extent.height : extent.width;
}

[[nodiscard]] auto saturated_add(int left, int right) noexcept -> int {
    if (right > std::numeric_limits<int>::max() - left)
        return std::numeric_limits<int>::max();
    return left + right;
}

} // namespace

Container::Container(Axis axis, int spacing) : axis_{axis}, spacing_{spacing} {
    if (spacing < 0)
        throw std::invalid_argument{"container spacing cannot be negative"};
}

void Container::add(Widget& child, int grow) {
    if (grow < 0)
        throw std::invalid_argument{"container growth weight cannot be negative"};
    children_.push_back({.widget = &child, .grow = grow});
}

void Container::clear() noexcept {
    children_.clear();
}

auto Container::measure(Constraints constraints) const -> Extent {
    int main = 0;
    int cross = 0;
    const Constraints child_constraints{.maximum = constraints.maximum};
    for (const auto& child : children_) {
        const auto desired = child.widget->measure(child_constraints);
        main = saturated_add(main, main_extent(desired, axis_));
        cross = std::max(cross, cross_extent(desired, axis_));
    }
    if (children_.size() > 1) {
        const auto gap_count = static_cast<std::int64_t>(children_.size() - 1);
        const auto gap_extent = std::min<std::int64_t>(std::numeric_limits<int>::max(), gap_count * spacing_);
        main = saturated_add(main, static_cast<int>(gap_extent));
    }
    const Extent desired = axis_ == Axis::horizontal ? Extent{main, cross} : Extent{cross, main};
    return constraints.constrain(desired);
}

void Container::layout(Rect bounds) {
    Widget::layout(bounds);
    const auto area = Widget::bounds();
    if (children_.empty())
        return;

    const int main = axis_ == Axis::horizontal ? area.width : area.height;
    const int cross = axis_ == Axis::horizontal ? area.height : area.width;
    const int gap_count = static_cast<int>(children_.size() - 1);
    const int gap = gap_count == 0 ? 0 : std::min(spacing_, main / gap_count);
    const int available = std::max(0, main - gap * gap_count);
    const Constraints child_constraints{.maximum = {.width = area.width, .height = area.height}};

    std::vector<int> allocation;
    allocation.reserve(children_.size());
    std::int64_t preferred_total = 0;
    int total_growth = 0;
    for (const auto& child : children_) {
        const int preferred = std::max(0, main_extent(child.widget->measure(child_constraints), axis_));
        allocation.push_back(preferred);
        preferred_total += preferred;
        total_growth = saturated_add(total_growth, child.grow);
    }

    if (preferred_total > available && preferred_total > 0) {
        const auto preferred = allocation;
        std::ranges::fill(allocation, 0);
        const int positive_children =
            static_cast<int>(std::ranges::count_if(preferred, [](int extent) { return extent > 0; }));
        int remaining = available;
        if (available >= positive_children) {
            for (std::size_t index = 0; index < preferred.size(); ++index) {
                if (preferred[index] > 0) {
                    allocation[index] = 1;
                    --remaining;
                }
            }
            const auto remaining_weight = preferred_total - positive_children;
            int distributed = 0;
            if (remaining_weight > 0) {
                for (std::size_t index = 0; index < preferred.size(); ++index) {
                    const int extra = static_cast<int>(static_cast<std::int64_t>(remaining) *
                                                       std::max(0, preferred[index] - 1) / remaining_weight);
                    allocation[index] += extra;
                    distributed += extra;
                }
            }
            int undistributed = remaining - distributed;
            for (std::size_t index = 0; undistributed > 0 && index < allocation.size(); ++index) {
                if (allocation[index] < preferred[index]) {
                    ++allocation[index];
                    --undistributed;
                }
            }
        } else {
            for (std::size_t index = 0; remaining > 0 && index < preferred.size(); ++index) {
                if (preferred[index] > 0) {
                    allocation[index] = 1;
                    --remaining;
                }
            }
        }
    } else if (preferred_total < available && total_growth > 0) {
        int remainder = available - static_cast<int>(preferred_total);
        int distributed = 0;
        for (std::size_t index = 0; index < children_.size(); ++index) {
            const int extra =
                static_cast<int>(static_cast<std::int64_t>(remainder) * children_[index].grow / total_growth);
            allocation[index] += extra;
            distributed += extra;
        }
        int undistributed = remainder - distributed;
        for (std::size_t index = 0; undistributed > 0 && index < children_.size(); ++index) {
            if (children_[index].grow > 0) {
                ++allocation[index];
                --undistributed;
            }
        }
    }

    int position = axis_ == Axis::horizontal ? area.x : area.y;
    for (std::size_t index = 0; index < children_.size(); ++index) {
        const Rect child_bounds = axis_ == Axis::horizontal
                                      ? Rect{.x = position, .y = area.y, .width = allocation[index], .height = cross}
                                      : Rect{.x = area.x, .y = position, .width = cross, .height = allocation[index]};
        children_[index].widget->layout(child_bounds);
        position += allocation[index] + gap;
    }
}

void Container::render(terminal::ScreenBuffer& buffer) const {
    for (const auto& child : children_)
        child.widget->render(buffer);
}

} // namespace vulpes::ui
