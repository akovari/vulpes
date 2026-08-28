#pragma once

#include "vulpes/terminal/terminal.hpp"
#include "vulpes/ui/document_surface.hpp"

#include <string>
#include <utility>

namespace vulpes::ui {

// Hosts one semantic document in a terminal. This is used by direct CLI modes
// and deliberately keeps raw mode, screen presentation, and key mapping out
// of browse and SQL document implementations.
class DocumentSession {
  public:
    DocumentSession(terminal::Terminal& terminal, DocumentSurface& surface, terminal::Size minimum_size,
                    std::string too_small_message)
        : terminal_{&terminal}, surface_{&surface}, minimum_size_{minimum_size},
          too_small_message_{std::move(too_small_message)} {}

    void run();

  private:
    terminal::Terminal* terminal_;
    DocumentSurface* surface_;
    terminal::Size minimum_size_;
    std::string too_small_message_;
};

} // namespace vulpes::ui
