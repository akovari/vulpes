#pragma once

#include "vulpes/terminal/terminal.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace vulpes::core {

// Stable action IDs are independent of terminal escape sequences and localized
// labels. Frontends may replace the default bindings without changing behavior.
enum class ActionId {
    none,
    application_back,
    application_quit,
    application_menu,
    application_command_palette,
    database_open,
    database_open_read_only,
    database_create,
    workspace_next_document,
    workspace_close_document,
    dataset_previous,
    dataset_next,
    dataset_first,
    dataset_last,
    grid_previous_column,
    grid_next_column,
    record_new,
    record_edit,
    record_delete,
    dataset_search,
    dataset_filter,
    dataset_refresh,
    dataset_sort,
};

struct KeyBinding {
    terminal::KeyEvent key;
    ActionId action{ActionId::none};
};

class ActionMap {
  public:
    ActionMap();
    explicit ActionMap(std::vector<KeyBinding> bindings);

    void bind(KeyBinding binding);
    [[nodiscard]] auto action_for(const terminal::InputEvent& event) const -> ActionId;
    [[nodiscard]] auto bindings() const noexcept -> const std::vector<KeyBinding>& { return bindings_; }

  private:
    std::vector<KeyBinding> bindings_;
};

[[nodiscard]] auto action_id(ActionId action) -> std::string_view;

} // namespace vulpes::core
