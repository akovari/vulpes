#pragma once

#include <algorithm>
#include <limits>

namespace vulpes::ui {

struct Extent {
    int width{};
    int height{};
    auto operator==(const Extent&) const -> bool = default;
};

struct Constraints {
    Extent minimum{};
    Extent maximum{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};

    [[nodiscard]] auto constrain(Extent desired) const noexcept -> Extent {
        const int minimum_width = std::max(0, minimum.width);
        const int minimum_height = std::max(0, minimum.height);
        const int maximum_width = std::max(minimum_width, maximum.width);
        const int maximum_height = std::max(minimum_height, maximum.height);
        return {
            .width = std::clamp(std::max(0, desired.width), minimum_width, maximum_width),
            .height = std::clamp(std::max(0, desired.height), minimum_height, maximum_height),
        };
    }
};

enum class Axis { horizontal, vertical };
enum class Alignment { start, center, end };

} // namespace vulpes::ui
