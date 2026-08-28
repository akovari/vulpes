#include "vulpes/ui/directory_browser.hpp"

#include "vulpes/terminal/unicode.hpp"
#include "vulpes/ui/window_frame.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace vulpes::ui {
namespace {

auto path_text(const std::filesystem::path& path) -> std::string {
    const auto encoded = path.generic_u8string();
    return {encoded.begin(), encoded.end()};
}

auto lowercase_ascii(std::string value) -> std::string {
    for (auto& character : value) {
        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');
    }
    return value;
}

void write(terminal::ScreenBuffer& buffer, int x, int y, int width, std::string_view text, terminal::Style style = {}) {
    const auto end = buffer.write_utf8(x, y, terminal::truncate_utf8(text, width), style);
    for (int column = end; column < x + width; ++column)
        buffer.put(column, y, U' ', style);
}

} // namespace

DirectoryBrowser::DirectoryBrowser(std::filesystem::path initial_directory, std::string title, std::string instructions,
                                   std::string parent_label)
    : title_{std::move(title)}, instructions_{std::move(instructions)}, parent_label_{std::move(parent_label)} {
    std::error_code error;
    if (!std::filesystem::is_directory(initial_directory, error))
        initial_directory = initial_directory.parent_path();
    if (initial_directory.empty())
        initial_directory = std::filesystem::current_path(error);
    load(std::move(initial_directory));
}

void DirectoryBrowser::load(std::filesystem::path directory) {
    entries_.clear();
    selected_ = 0;
    selected_path_.reset();
    error_.clear();

    std::error_code error;
    const auto normalized = std::filesystem::absolute(std::move(directory), error).lexically_normal();
    if (error) {
        error_ = error.message();
        return;
    }
    directory_ = normalized;
    const auto parent = directory_.parent_path();
    if (!parent.empty() && parent != directory_)
        entries_.push_back({.path = parent, .label = parent_label_, .directory = true, .parent = true});

    std::filesystem::directory_iterator iterator{directory_, std::filesystem::directory_options::skip_permission_denied,
                                                 error};
    if (error) {
        error_ = error.message();
        return;
    }
    const auto end = std::filesystem::directory_iterator{};
    while (iterator != end) {
        const auto item = *iterator;
        iterator.increment(error);
        if (error) {
            error_ = error.message();
            break;
        }
        std::error_code type_error;
        const bool is_directory = item.is_directory(type_error);
        if (type_error)
            continue;
        const bool is_regular = item.is_regular_file(type_error);
        if (type_error || (!is_directory && !is_regular))
            continue;
        const auto name = path_text(item.path().filename());
        entries_.push_back(
            {.path = item.path(), .label = is_directory ? "[" + name + "]" : name, .directory = is_directory});
    }
    auto first_file = entries_.begin();
    if (!entries_.empty() && entries_.front().parent)
        ++first_file;
    std::ranges::sort(first_file, entries_.end(), [](const Entry& left, const Entry& right) {
        if (left.directory != right.directory)
            return left.directory > right.directory;
        return lowercase_ascii(left.label) < lowercase_ascii(right.label);
    });
}

void DirectoryBrowser::move_selection(int direction) {
    if (entries_.empty())
        return;
    const auto count = static_cast<int>(entries_.size());
    selected_ = static_cast<std::size_t>((static_cast<int>(selected_) + direction + count) % count);
}

void DirectoryBrowser::select_by_prefix(char32_t character) {
    if (entries_.empty() || character > 0x7F)
        return;
    const auto prefix = static_cast<char>(character >= U'A' && character <= U'Z' ? character - U'A' + U'a' : character);
    for (std::size_t offset = 1; offset <= entries_.size(); ++offset) {
        const auto index = (selected_ + offset) % entries_.size();
        const auto label = lowercase_ascii(entries_[index].label);
        if (!label.empty() && label.front() == prefix) {
            selected_ = index;
            return;
        }
    }
}

auto DirectoryBrowser::handle(const terminal::InputEvent& event) -> DirectoryBrowserResult {
    const auto* key = std::get_if<terminal::KeyEvent>(&event);
    if (key == nullptr)
        return DirectoryBrowserResult::redraw;
    if (key->key == terminal::Key::escape)
        return DirectoryBrowserResult::cancelled;
    if (key->key == terminal::Key::up) {
        move_selection(-1);
        return DirectoryBrowserResult::redraw;
    }
    if (key->key == terminal::Key::down) {
        move_selection(1);
        return DirectoryBrowserResult::redraw;
    }
    if (key->key == terminal::Key::home) {
        selected_ = 0;
        return DirectoryBrowserResult::redraw;
    }
    if (key->key == terminal::Key::end && !entries_.empty()) {
        selected_ = entries_.size() - 1;
        return DirectoryBrowserResult::redraw;
    }
    if (key->key == terminal::Key::backspace) {
        const auto parent = directory_.parent_path();
        if (!parent.empty() && parent != directory_)
            load(parent);
        return DirectoryBrowserResult::redraw;
    }
    if (key->key == terminal::Key::character && !key->ctrl && !key->alt) {
        select_by_prefix(key->character);
        return DirectoryBrowserResult::redraw;
    }
    if (key->key != terminal::Key::enter || entries_.empty())
        return DirectoryBrowserResult::unchanged;

    const auto& entry = entries_[selected_];
    if (entry.directory) {
        load(entry.path);
        return DirectoryBrowserResult::redraw;
    }
    selected_path_ = entry.path;
    return DirectoryBrowserResult::selected;
}

void DirectoryBrowser::render(terminal::ScreenBuffer& buffer, Rect bounds) const {
    if (!WindowFrame::fits(buffer, bounds, 30, 8))
        return;
    WindowFrame::render(buffer, bounds, title_);
    const auto content = WindowFrame::content_bounds(bounds);
    write(buffer, content.x, content.y, content.width, path_text(directory_));
    const auto visible_count = static_cast<std::size_t>(std::max(0, content.height - 3));
    auto first_visible = std::size_t{0};
    if (visible_count > 0) {
        if (selected_ < first_visible)
            first_visible = selected_;
        else if (selected_ >= first_visible + visible_count)
            first_visible = selected_ - visible_count + 1;
    }
    for (std::size_t row = 0; row < visible_count && first_visible + row < entries_.size(); ++row) {
        const auto index = first_visible + row;
        write(buffer, content.x, content.y + 1 + static_cast<int>(row), content.width, entries_[index].label,
              {.reverse = index == selected_});
    }
    write(buffer, content.x, content.y + content.height - 1, content.width, error_.empty() ? instructions_ : error_);
}

} // namespace vulpes::ui
