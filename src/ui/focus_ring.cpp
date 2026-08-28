#include "vulpes/ui/focus_ring.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace vulpes::ui {

FocusRing::FocusRing(std::vector<bool> focusable) {
    reset(std::move(focusable));
}

void FocusRing::reset(std::vector<bool> focusable) {
    focusable_ = std::move(focusable);
    const auto first = std::ranges::find(focusable_, true);
    if (first == focusable_.end()) {
        current_.reset();
        return;
    }
    current_ = static_cast<std::size_t>(std::distance(focusable_.begin(), first));
}

auto FocusRing::current() const noexcept -> std::optional<std::size_t> {
    return current_;
}

auto FocusRing::select(std::size_t index) -> bool {
    if (index >= focusable_.size() || !focusable_[index] || current_ == index)
        return false;
    current_ = index;
    return true;
}

auto FocusRing::move(int direction) -> bool {
    if (!current_ || direction == 0)
        return false;

    const auto count = static_cast<int>(focusable_.size());
    const int step = direction < 0 ? -1 : 1;
    auto candidate = static_cast<int>(*current_);
    do {
        candidate = (candidate + step + count) % count;
        if (focusable_[static_cast<std::size_t>(candidate)]) {
            if (static_cast<std::size_t>(candidate) == *current_)
                return false;
            current_ = static_cast<std::size_t>(candidate);
            return true;
        }
    } while (static_cast<std::size_t>(candidate) != *current_);
    return false;
}

} // namespace vulpes::ui
