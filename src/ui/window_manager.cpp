#include "vulpes/ui/window_manager.hpp"

#include "vulpes/terminal/unicode.hpp"

#include <algorithm>

namespace vulpes::ui {
namespace {
void write(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text, terminal::Style style) {
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

WindowManager::WindowManager(const Theme& theme, std::string workspace_title) : theme_{&theme} {
    documents_.push_back(
        {.id = "workspace", .title = std::move(workspace_title), .kind = DocumentKind::workspace, .closable = false});
}

void WindowManager::open(Document document) {
    const auto existing = std::ranges::find(documents_, document.id, &Document::id);
    if (existing != documents_.end()) {
        active_index_ = static_cast<std::size_t>(std::distance(documents_.begin(), existing));
        return;
    }
    documents_.push_back(std::move(document));
    active_index_ = documents_.size() - 1;
}

auto WindowManager::close_active() -> bool {
    if (!active().closable)
        return false;
    documents_.erase(documents_.begin() + static_cast<std::ptrdiff_t>(active_index_));
    active_index_ = std::min(active_index_, documents_.size() - 1);
    return true;
}

auto WindowManager::active() const -> const Document& {
    return documents_.at(active_index_);
}
void WindowManager::show_modal(std::string title) {
    modal_title_ = std::move(title);
}
void WindowManager::dismiss_modal() noexcept {
    modal_title_.reset();
}

auto WindowManager::handle(core::ActionId action) -> bool {
    if (modal_title_) {
        if (action == core::ActionId::application_back) {
            dismiss_modal();
            return true;
        }
        return false;
    }
    if (action == core::ActionId::workspace_next_document && documents_.size() > 1) {
        active_index_ = (active_index_ + 1) % documents_.size();
        return true;
    }
    return false;
}

void WindowManager::render_tabs(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (bounds.width < 8 || bounds.height < 1)
        return;
    write(buffer, bounds.x, bounds.y, bounds.width, "", theme_->style(ThemeRole::tab));
    int x = bounds.x + 1;
    for (std::size_t index = 0; index < documents_.size() && x < bounds.x + bounds.width; ++index) {
        const auto label = " " + documents_[index].title + (documents_[index].closable ? " x " : " ");
        const int width = std::min(static_cast<int>(label.size()), bounds.x + bounds.width - x);
        write(buffer, x, bounds.y, width, label,
              index == active_index_ ? theme_->style(ThemeRole::active_tab) : theme_->style(ThemeRole::tab));
        x += width;
    }
}

} // namespace vulpes::ui
