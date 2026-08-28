#pragma once

#include "vulpes/core/localization.hpp"
#include "vulpes/ui/document_surface.hpp"

#include <string>
#include <vector>

namespace vulpes::ui {

// A host-verification surface that exposes only Vulpes' normalized input
// events. It intentionally does not reveal terminal-library event codes.
class TerminalDiagnostics final : public DocumentSurface {
  public:
    explicit TerminalDiagnostics(const core::Localizer& messages);

    [[nodiscard]] auto handle(core::ActionId action, const terminal::InputEvent& event) -> DocumentResult override;
    void render(terminal::ScreenBuffer& buffer, Rect bounds) override;

  private:
    void append(std::string event);

    std::string title_;
    std::string instructions_;
    std::string waiting_;
    std::vector<std::string> events_;
};

} // namespace vulpes::ui
