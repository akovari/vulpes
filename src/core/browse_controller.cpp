#include "vulpes/core/browse_controller.hpp"

namespace vulpes::core {

auto BrowseController::handle(const terminal::InputEvent& event) -> BrowseResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key == nullptr)
        return BrowseResult::redraw;
    switch (key->key) {
    case terminal::Key::escape:
    case terminal::Key::character:
        if (key->key == terminal::Key::escape || (key->ctrl && (key->character == U'c' || key->character == U'C')))
            return BrowseResult::close;
        return BrowseResult::unchanged;
    case terminal::Key::up:
        return dataset_->previous() ? BrowseResult::redraw : BrowseResult::unchanged;
    case terminal::Key::down:
        return dataset_->next() ? BrowseResult::redraw : BrowseResult::unchanged;
    case terminal::Key::home:
        return dataset_->first() ? BrowseResult::redraw : BrowseResult::unchanged;
    case terminal::Key::end:
        return dataset_->last() ? BrowseResult::redraw : BrowseResult::unchanged;
    case terminal::Key::page_up:
        return dataset_->first() ? BrowseResult::redraw : BrowseResult::unchanged;
    case terminal::Key::page_down:
        return dataset_->last() ? BrowseResult::redraw : BrowseResult::unchanged;
    default:
        return BrowseResult::unchanged;
    }
}

} // namespace vulpes::core
