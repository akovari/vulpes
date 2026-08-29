#pragma once

#include "vulpes/core/actions.hpp"
#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/geometry.hpp"

namespace vulpes::ui {

// The frontend-neutral contract for an interactive workspace surface. A
// surface owns its semantic state; a host owns terminal lifecycle and routing.
// A document can request a semantic application command, but the workspace
// host remains responsible for parsing and dispatching it through the runtime.
enum class DocumentResult { unchanged, redraw, command, close };

class DocumentSurface {
  public:
    virtual ~DocumentSurface() = default;

    [[nodiscard]] virtual auto is_dirty() const noexcept -> bool { return false; }
    [[nodiscard]] virtual auto handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult = 0;
    virtual void render(terminal::ScreenBuffer& buffer, Rect bounds) = 0;
};

} // namespace vulpes::ui
