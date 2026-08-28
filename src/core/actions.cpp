#include "vulpes/core/actions.hpp"

#include <algorithm>
#include <utility>

namespace vulpes::core {
namespace {

auto matches(const terminal::KeyEvent& left, const terminal::KeyEvent& right) -> bool {
    return left.key == right.key && left.character == right.character && left.ctrl == right.ctrl &&
           left.alt == right.alt && left.shift == right.shift;
}

auto default_bindings() -> std::vector<KeyBinding> {
    using terminal::Key;
    return {
        {{.key = Key::escape}, ActionId::application_back},
        {{.key = Key::character, .character = U'c', .ctrl = true}, ActionId::application_quit},
        {{.key = Key::f10}, ActionId::application_menu},
        {{.key = Key::character, .character = U'f', .alt = true}, ActionId::application_menu},
        {{.key = Key::character, .character = U'p', .ctrl = true}, ActionId::application_command_palette},
        {{.key = Key::character, .character = U'o', .ctrl = true}, ActionId::database_open},
        {{.key = Key::character, .character = U'r', .ctrl = true}, ActionId::database_open_read_only},
        {{.key = Key::character, .character = U'n', .ctrl = true}, ActionId::database_create},
        {{.key = Key::tab, .ctrl = true}, ActionId::workspace_next_document},
        {{.key = Key::character, .character = U'w', .ctrl = true}, ActionId::workspace_close_document},
        {{.key = Key::f7}, ActionId::document_switch_pane},
        {{.key = Key::up}, ActionId::dataset_previous},
        {{.key = Key::down}, ActionId::dataset_next},
        {{.key = Key::home}, ActionId::dataset_first},
        {{.key = Key::end}, ActionId::dataset_last},
        {{.key = Key::page_up}, ActionId::dataset_first},
        {{.key = Key::page_down}, ActionId::dataset_last},
        {{.key = Key::left}, ActionId::grid_previous_column},
        {{.key = Key::right}, ActionId::grid_next_column},
        {{.key = Key::left, .ctrl = true}, ActionId::grid_narrow_column},
        {{.key = Key::right, .ctrl = true}, ActionId::grid_widen_column},
        {{.key = Key::insert_key}, ActionId::record_new},
        {{.key = Key::f2}, ActionId::record_edit},
        {{.key = Key::delete_key}, ActionId::record_delete},
        {{.key = Key::f3}, ActionId::dataset_search},
        {{.key = Key::f4}, ActionId::dataset_filter},
        {{.key = Key::f5}, ActionId::dataset_refresh},
        {{.key = Key::f6}, ActionId::dataset_sort},
    };
}

} // namespace

ActionMap::ActionMap() : bindings_{default_bindings()} {
}

ActionMap::ActionMap(std::vector<KeyBinding> bindings) : bindings_{std::move(bindings)} {
}

void ActionMap::bind(KeyBinding binding) {
    const auto existing =
        std::ranges::find_if(bindings_, [&](const auto& item) { return matches(item.key, binding.key); });
    if (existing == bindings_.end())
        bindings_.push_back(binding);
    else
        *existing = binding;
}

auto ActionMap::action_for(const terminal::InputEvent& event) const -> ActionId {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key == nullptr)
        return ActionId::none;
    const auto binding = std::ranges::find_if(bindings_, [&](const auto& item) { return matches(item.key, *key); });
    return binding == bindings_.end() ? ActionId::none : binding->action;
}

auto action_id(ActionId action) -> std::string_view {
    switch (action) {
    case ActionId::application_back:
        return "application.back";
    case ActionId::application_quit:
        return "application.quit";
    case ActionId::application_menu:
        return "application.menu";
    case ActionId::application_command_palette:
        return "application.command_palette";
    case ActionId::database_open:
        return "database.open";
    case ActionId::database_open_read_only:
        return "database.open_read_only";
    case ActionId::database_create:
        return "database.create";
    case ActionId::workspace_next_document:
        return "workspace.next_document";
    case ActionId::workspace_close_document:
        return "workspace.close_document";
    case ActionId::document_switch_pane:
        return "document.switch_pane";
    case ActionId::dataset_previous:
        return "dataset.previous";
    case ActionId::dataset_next:
        return "dataset.next";
    case ActionId::dataset_first:
        return "dataset.first";
    case ActionId::dataset_last:
        return "dataset.last";
    case ActionId::grid_previous_column:
        return "grid.previous_column";
    case ActionId::grid_next_column:
        return "grid.next_column";
    case ActionId::grid_narrow_column:
        return "grid.narrow_column";
    case ActionId::grid_widen_column:
        return "grid.widen_column";
    case ActionId::record_new:
        return "record.new";
    case ActionId::record_edit:
        return "record.edit";
    case ActionId::record_delete:
        return "record.delete";
    case ActionId::dataset_search:
        return "dataset.search";
    case ActionId::dataset_filter:
        return "dataset.filter";
    case ActionId::dataset_refresh:
        return "dataset.refresh";
    case ActionId::dataset_sort:
        return "dataset.sort";
    case ActionId::none:
        return {};
    }
    return {};
}

} // namespace vulpes::core
