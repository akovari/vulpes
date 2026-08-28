#pragma once

#include "vulpes/ui/widget.hpp"

#include <cstddef>
#include <vector>

namespace vulpes::ui {

// A non-owning linear layout. Children must outlive the container; this keeps
// semantic screens free to own controls directly while centralizing geometry.
class Container final : public Widget {
  public:
    explicit Container(Axis axis, int spacing = 0);

    void add(Widget& child, int grow = 0);
    void clear() noexcept;
    [[nodiscard]] auto child_count() const noexcept -> std::size_t { return children_.size(); }

    [[nodiscard]] auto measure(Constraints constraints = {}) const -> Extent override;
    void layout(Rect bounds) override;
    void render(terminal::ScreenBuffer& buffer) const override;

  private:
    struct Child {
        Widget* widget;
        int grow;
    };

    Axis axis_;
    int spacing_;
    std::vector<Child> children_;
};

} // namespace vulpes::ui
