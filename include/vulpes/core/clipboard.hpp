#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace vulpes::core {

// Frontend service boundary for UTF-8 clipboard text. Editors depend on this
// interface rather than host APIs, keeping their behavior deterministic in
// tests and portable to future GUI or web frontends.
class Clipboard {
  public:
    virtual ~Clipboard() = default;
    [[nodiscard]] virtual auto read_text() -> std::optional<std::string> = 0;
    [[nodiscard]] virtual auto write_text(std::string_view text) -> bool = 0;
};

class SystemClipboard final : public Clipboard {
  public:
    [[nodiscard]] auto read_text() -> std::optional<std::string> override;
    [[nodiscard]] auto write_text(std::string_view text) -> bool override;
};

} // namespace vulpes::core
