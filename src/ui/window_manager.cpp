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
void WindowManager::set_active_status(std::string status) {
    documents_.at(active_index_).status = std::move(status);
}
void WindowManager::set_active_dirty(bool dirty) noexcept {
    documents_[active_index_].dirty = dirty;
}
auto WindowManager::active_status() const -> std::string_view {
    return active().status;
}
void WindowManager::reset_documents() {
    documents_.erase(documents_.begin() + 1, documents_.end());
    active_index_ = 0;
    dismiss_all_modals();
}
void WindowManager::show_modal(std::string title) {
    modal_titles_.push_back(std::move(title));
}
void WindowManager::dismiss_modal() noexcept {
    if (!modal_titles_.empty())
        modal_titles_.pop_back();
}
void WindowManager::dismiss_all_modals() noexcept {
    modal_titles_.clear();
}
auto WindowManager::modal_title() const noexcept -> std::optional<std::string_view> {
    return modal_titles_.empty() ? std::nullopt : std::optional<std::string_view>{modal_titles_.back()};
}

auto WindowManager::handle(core::ActionId action) -> bool {
    if (!modal_titles_.empty()) {
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
    constexpr int minimum_tab_width = 8;
    const auto maximum_visible = std::max(std::size_t{1}, static_cast<std::size_t>(bounds.width / minimum_tab_width));
    const auto visible_count = std::min(maximum_visible, documents_.size());
    const auto first_visible = active_index_ < visible_count ? std::size_t{0} : active_index_ - visible_count + 1;
    int x = bounds.x;
    for (std::size_t index = first_visible; index < first_visible + visible_count; ++index) {
        const auto remaining_tabs = static_cast<int>(first_visible + visible_count - index);
        const auto available = bounds.x + bounds.width - x;
        const auto fair_width = available / remaining_tabs;
        const auto displayed_title = documents_[index].title + (documents_[index].dirty ? " *" : "");
        const auto preferred = terminal::text_width(displayed_title) + (documents_[index].closable ? 5 : 3);
        const int width = std::min({24, fair_width, std::max(minimum_tab_width, preferred)});
        const auto label = " " + displayed_title + (documents_[index].closable ? " × " : " ");
        write(buffer, x, bounds.y, width, label,
              index == active_index_ ? theme_->style(ThemeRole::active_tab) : theme_->style(ThemeRole::tab));
        x += width;
        if (x < bounds.x + bounds.width)
            buffer.put(x++, bounds.y, U'│', theme_->style(ThemeRole::tab));
    }
}

} // namespace vulpes::ui
