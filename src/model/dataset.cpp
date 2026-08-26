#include "vulpes/model/dataset.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/identifier.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace vulpes::model {
namespace {

[[nodiscard]] auto is_text_field(const db::FieldSchema& field) -> bool {
    auto type = field.declared_type;
    std::ranges::transform(type, type.begin(),
                           [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
    return type.find("CHAR") != std::string::npos || type.find("CLOB") != std::string::npos ||
           type.find("TEXT") != std::string::npos;
}

[[nodiscard]] auto operator_sql(FilterOperator comparison) -> std::string_view {
    switch (comparison) {
    case FilterOperator::equal:
        return " = ";
    case FilterOperator::not_equal:
        return " <> ";
    case FilterOperator::less:
        return " < ";
    case FilterOperator::less_equal:
        return " <= ";
    case FilterOperator::greater:
        return " > ";
    case FilterOperator::greater_equal:
        return " >= ";
    }
    return {};
}

[[nodiscard]] auto escape_like_pattern(std::string_view text) -> std::string {
    std::string pattern;
    pattern.reserve(text.size() + 2);
    pattern += '%';
    for (const char character : text) {
        if (character == '%' || character == '_' || character == '\\')
            pattern += '\\';
        pattern += character;
    }
    pattern += '%';
    return pattern;
}

} // namespace

Dataset::Dataset(db::Database& database, db::TableSchema schema, std::size_t page_size)
    : database_{&database}, schema_{std::move(schema)}, page_size_{page_size} {
    if (schema_.name.empty())
        throw Error{ErrorCategory::database, "dataset requires a table or view name"};
    if (page_size_ == 0)
        throw Error{ErrorCategory::database, "dataset page size must be greater than zero"};
    refresh();
}

auto Dataset::total_count() -> std::size_t {
    std::vector<db::Value> values;
    auto query = database_->prepare("SELECT count(*) FROM " + db::detail::quote_identifier(schema_.name) +
                                    query_where_clause(values));
    for (std::size_t index = 0; index < values.size(); ++index)
        query.bind(static_cast<int>(index + 1), values[index]);
    if (!query.step())
        return 0;
    return static_cast<std::size_t>(query.column(0).as_int());
}

auto Dataset::current() const -> std::optional<db::Row> {
    if (rows_.empty())
        return std::nullopt;
    return rows_.at(current_index_);
}

auto Dataset::current_row_index() const -> std::optional<std::size_t> {
    if (rows_.empty())
        return std::nullopt;
    return current_index_;
}

auto Dataset::current_identity() const -> std::optional<RowIdentity> {
    const auto row = current();
    const auto fields = schema_.primary_key_fields();
    if (!row || fields.empty())
        return std::nullopt;

    RowIdentity identity;
    identity.fields = fields;
    for (const auto& field : identity.fields)
        identity.values.push_back(row->at(field));
    return identity;
}

auto Dataset::is_editable() const noexcept -> bool {
    return !schema_.is_view && !schema_.primary_key_fields().empty();
}

auto Dataset::order_by(std::string_view field, SortDirection direction) -> Dataset& {
    validate_field(field);
    order_ = std::pair{std::string{field}, direction};
    page_offset_ = 0;
    refresh();
    return *this;
}

auto Dataset::where(Filter filter) -> Dataset& {
    validate_field(filter.field);
    if (filter.value.is_null() && filter.comparison != FilterOperator::equal &&
        filter.comparison != FilterOperator::not_equal) {
        throw Error{ErrorCategory::validation, "NULL can only be compared using equal or not_equal"};
    }
    filters_.push_back(std::move(filter));
    page_offset_ = 0;
    refresh();
    return *this;
}

auto Dataset::search(std::string_view text) -> Dataset& {
    search_ = std::string{text};
    page_offset_ = 0;
    refresh();
    return *this;
}

auto Dataset::clear_filters() -> Dataset& {
    filters_.clear();
    page_offset_ = 0;
    refresh();
    return *this;
}

auto Dataset::clear_search() -> Dataset& {
    search_.reset();
    page_offset_ = 0;
    refresh();
    return *this;
}

void Dataset::refresh() {
    std::vector<db::Value> values;
    const auto sql = "SELECT * FROM " + db::detail::quote_identifier(schema_.name) + query_where_clause(values) +
                     order_clause() + " LIMIT ? OFFSET ?";
    auto query = database_->prepare(sql);
    int index = 1;
    for (const auto& value : values)
        query.bind(index++, value);
    query.bind(index++, db::Value{static_cast<std::int64_t>(page_size_)});
    query.bind(index, db::Value{static_cast<std::int64_t>(page_offset_)});

    rows_.clear();
    while (query.step())
        rows_.push_back(query.row());
    current_index_ = 0;
}

auto Dataset::first() -> bool {
    page_offset_ = 0;
    refresh();
    return !rows_.empty();
}

auto Dataset::next() -> bool {
    if (rows_.empty())
        return false;
    if (current_index_ + 1 < rows_.size()) {
        ++current_index_;
        return true;
    }
    if (page_offset_ + rows_.size() >= total_count())
        return false;
    page_offset_ += rows_.size();
    refresh();
    return !rows_.empty();
}

auto Dataset::previous() -> bool {
    if (rows_.empty())
        return false;
    if (current_index_ > 0) {
        --current_index_;
        return true;
    }
    if (page_offset_ == 0)
        return false;
    page_offset_ = page_offset_ > page_size_ ? page_offset_ - page_size_ : 0;
    refresh();
    current_index_ = rows_.empty() ? 0 : rows_.size() - 1;
    return !rows_.empty();
}

auto Dataset::last() -> bool {
    const auto count = total_count();
    if (count == 0) {
        rows_.clear();
        current_index_ = 0;
        page_offset_ = 0;
        return false;
    }
    page_offset_ = ((count - 1) / page_size_) * page_size_;
    refresh();
    current_index_ = rows_.size() - 1;
    return true;
}

void Dataset::validate_field(std::string_view field) const {
    const auto found = std::ranges::find(schema_.fields, field, &db::FieldSchema::name);
    if (found == schema_.fields.end())
        throw Error{ErrorCategory::validation, "unknown dataset field: " + std::string{field}};
}

auto Dataset::query_where_clause(std::vector<db::Value>& values) const -> std::string {
    std::vector<std::string> predicates;
    for (const auto& filter : filters_) {
        const auto field = db::detail::quote_identifier(filter.field);
        if (filter.value.is_null()) {
            predicates.push_back(field + (filter.comparison == FilterOperator::equal ? " IS NULL" : " IS NOT NULL"));
        } else {
            predicates.push_back(field + std::string{operator_sql(filter.comparison)} + "?");
            values.push_back(filter.value);
        }
    }
    if (search_ && !search_->empty() && has_text_fields()) {
        std::vector<std::string> text_predicates;
        for (const auto& field : schema_.fields) {
            if (!field.hidden && is_text_field(field))
                text_predicates.push_back(db::detail::quote_identifier(field.name) + " LIKE ? ESCAPE '\\'");
        }
        predicates.push_back("(" + [&] {
            std::string combined;
            for (std::size_t index = 0; index < text_predicates.size(); ++index) {
                if (index != 0)
                    combined += " OR ";
                combined += text_predicates[index];
                values.emplace_back(escape_like_pattern(*search_));
            }
            return combined;
        }() + ")");
    }
    if (predicates.empty())
        return {};
    std::string clause{" WHERE "};
    for (std::size_t index = 0; index < predicates.size(); ++index) {
        if (index != 0)
            clause += " AND ";
        clause += predicates[index];
    }
    return clause;
}

auto Dataset::order_clause() const -> std::string {
    if (order_) {
        return " ORDER BY " + db::detail::quote_identifier(order_->first) +
               (order_->second == SortDirection::ascending ? " ASC" : " DESC");
    }
    const auto primary_key = schema_.primary_key_fields();
    if (primary_key.empty())
        return {};
    std::string clause{" ORDER BY "};
    for (std::size_t index = 0; index < primary_key.size(); ++index) {
        if (index != 0)
            clause += ", ";
        clause += db::detail::quote_identifier(primary_key[index]) + " ASC";
    }
    return clause;
}

auto Dataset::has_text_fields() const -> bool {
    return std::ranges::any_of(schema_.fields, [](const auto& field) { return !field.hidden && is_text_field(field); });
}

} // namespace vulpes::model
