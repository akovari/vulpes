#include "vulpes/core/browse_controller.hpp"

namespace vulpes::core {

auto BrowseController::handle(ActionId action) -> BrowseResult {
    switch (action) {
    case ActionId::application_back:
    case ActionId::application_quit:
        return BrowseResult::close;
    case ActionId::dataset_previous:
        return dataset_->previous() ? BrowseResult::redraw : BrowseResult::unchanged;
    case ActionId::dataset_next:
        return dataset_->next() ? BrowseResult::redraw : BrowseResult::unchanged;
    case ActionId::dataset_first:
        return dataset_->first() ? BrowseResult::redraw : BrowseResult::unchanged;
    case ActionId::dataset_last:
        return dataset_->last() ? BrowseResult::redraw : BrowseResult::unchanged;
    default:
        return BrowseResult::unchanged;
    }
}

} // namespace vulpes::core
