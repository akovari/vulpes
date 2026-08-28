#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace vulpes::ui {

enum class WindowLayerKind { prompt, dialog, form, lookup, drill_down };

struct WindowLayerDescriptor {
    std::string id;
    std::string title;
    WindowLayerKind kind{WindowLayerKind::dialog};
    bool dirty{false};
};

// An owning last-opened-first-routed semantic window stack. Content is chosen
// by the hosting surface (normally a std::variant), so this class has no widget,
// database, or terminal dependencies.
template <typename Content> class WindowStack {
  public:
    struct Layer {
        WindowLayerDescriptor descriptor;
        Content content;
    };

    void push(WindowLayerDescriptor descriptor, Content content) {
        layers_.push_back({.descriptor = std::move(descriptor), .content = std::move(content)});
    }

    [[nodiscard]] auto pop() noexcept -> bool {
        if (layers_.empty())
            return false;
        layers_.pop_back();
        return true;
    }

    void clear() noexcept { layers_.clear(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return layers_.empty(); }
    [[nodiscard]] auto size() const noexcept -> std::size_t { return layers_.size(); }
    [[nodiscard]] auto top() -> Layer& { return layers_.back(); }
    [[nodiscard]] auto top() const -> const Layer& { return layers_.back(); }
    [[nodiscard]] auto layers() noexcept -> std::vector<Layer>& { return layers_; }
    [[nodiscard]] auto layers() const noexcept -> const std::vector<Layer>& { return layers_; }

  private:
    std::vector<Layer> layers_;
};

} // namespace vulpes::ui
