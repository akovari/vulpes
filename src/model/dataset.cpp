#include "vulpes/model/dataset.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/db/database.hpp"
#include "vulpes/db/identifier.hpp"
#include "vulpes/db/transaction.hpp"

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

void Dataset::begin_insert() {
    ensure_editable();
    if (mode_ != DatasetMode::browse)
        throw Error{ErrorCategory::validation, "finish or cancel the current edit before inserting"};

    mode_ = DatasetMode::insert;
    draft_.assign(schema_.fields.size(), std::nullopt);
    modified_fields_.clear();
    original_identity_.reset();
}

void Dataset::begin_edit() {
    ensure_editable();
    if (mode_ != DatasetMode::browse)
        throw Error{ErrorCategory::validation, "finish or cancel the current edit before editing another record"};

    const auto row = current();
    const auto identity = current_identity();
    if (!row || !identity)
        throw Error{ErrorCategory::database, "the current record has no stable identity"};

    mode_ = DatasetMode::edit;
    draft_.clear();
    draft_.reserve(schema_.fields.size());
    for (const auto& field : schema_.fields)
        draft_.emplace_back(row->at(field.name));
    modified_fields_.clear();
    original_identity_ = identity;
}

auto Dataset::set(std::string_view field, db::Value value) -> Dataset& {
    ensure_editing();
    const auto index = field_index(field);
    const auto& field_schema = schema_.fields[index];
    if (field_schema.hidden || field_schema.generated)
        throw Error{ErrorCategory::validation, "field is not writable: " + field_schema.name};
    if (mode_ == DatasetMode::edit && field_schema.primary_key)
        throw Error{ErrorCategory::validation, "primary key fields cannot be edited"};
    if (!field_schema.nullable && value.is_null())
        throw Error{ErrorCategory::validation, "field cannot be NULL: " + field_schema.name};

    draft_[index] = std::move(value);
    if (std::ranges::find(modified_fields_, index) == modified_fields_.end())
        modified_fields_.push_back(index);
    return *this;
}

auto Dataset::draft_value(std::string_view field) const -> std::optional<db::Value> {
    const auto index = field_index(field);
    if (mode_ != DatasetMode::browse)
        return draft_.at(index);
    const auto row = current();
    if (!row)
        return std::nullopt;
    return row->at(index);
}

void Dataset::save() {
    ensure_editing();
    validate_draft();

    db::Transaction transaction{*database_};
    if (mode_ == DatasetMode::insert) {
        std::vector<std::size_t> fields;
        for (std::size_t index = 0; index < draft_.size(); ++index) {
            if (draft_[index])
                fields.push_back(index);
        }

        std::string sql{"INSERT INTO " + db::detail::quote_identifier(schema_.name)};
        if (fields.empty()) {
            sql += " DEFAULT VALUES";
            database_->execute(sql);
        } else {
            sql += " (";
            std::string placeholders;
            for (std::size_t position = 0; position < fields.size(); ++position) {
                if (position != 0) {
                    sql += ", ";
                    placeholders += ", ";
                }
                sql += db::detail::quote_identifier(schema_.fields[fields[position]].name);
                placeholders += '?';
            }
            sql += ") VALUES (" + placeholders + ')';
            auto statement = database_->prepare(sql);
            for (std::size_t position = 0; position < fields.size(); ++position)
                statement.bind(static_cast<int>(position + 1), *draft_[fields[position]]);
            statement.execute();
        }
    } else if (!modified_fields_.empty()) {
        std::string sql{"UPDATE " + db::detail::quote_identifier(schema_.name) + " SET "};
        for (std::size_t position = 0; position < modified_fields_.size(); ++position) {
            if (position != 0)
                sql += ", ";
            sql += db::detail::quote_identifier(schema_.fields[modified_fields_[position]].name) + " = ?";
        }

        std::vector<db::Value> identity_values;
        sql += " WHERE ";
        for (std::size_t position = 0; position < original_identity_->fields.size(); ++position) {
            if (position != 0)
                sql += " AND ";
            sql += db::detail::quote_identifier(original_identity_->fields[position]);
            const auto& value = original_identity_->values[position];
            if (value.is_null())
                sql += " IS NULL";
            else {
                sql += " = ?";
                identity_values.push_back(value);
            }
        }

        auto statement = database_->prepare(sql);
        int parameter = 1;
        for (const auto index : modified_fields_)
            statement.bind(parameter++, *draft_[index]);
        for (const auto& value : identity_values)
            statement.bind(parameter++, value);
        statement.execute();
        if (database_->changes() != 1)
            throw Error{ErrorCategory::database, "record was changed or deleted by another operation"};
    }
    transaction.commit();
    reset_edit_state();
    refresh_after_write();
}

void Dataset::cancel() noexcept {
    reset_edit_state();
}

void Dataset::erase() {
    ensure_editable();
    if (mode_ != DatasetMode::browse)
        throw Error{ErrorCategory::validation, "finish or cancel the current edit before deleting"};
    const auto identity = current_identity();
    if (!identity)
        throw Error{ErrorCategory::database, "the current record has no stable identity"};

    std::string sql{"DELETE FROM " + db::detail::quote_identifier(schema_.name) + " WHERE "};
    std::vector<db::Value> values;
    for (std::size_t position = 0; position < identity->fields.size(); ++position) {
        if (position != 0)
            sql += " AND ";
        sql += db::detail::quote_identifier(identity->fields[position]);
        const auto& value = identity->values[position];
        if (value.is_null())
            sql += " IS NULL";
        else {
            sql += " = ?";
            values.push_back(value);
        }
    }

    db::Transaction transaction{*database_};
    auto statement = database_->prepare(sql);
    for (std::size_t index = 0; index < values.size(); ++index)
        statement.bind(static_cast<int>(index + 1), values[index]);
    statement.execute();
    if (database_->changes() != 1)
        throw Error{ErrorCategory::database, "record was changed or deleted by another operation"};
    transaction.commit();
    refresh_after_write();
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

auto Dataset::field_index(std::string_view field) const -> std::size_t {
    const auto found = std::ranges::find(schema_.fields, field, &db::FieldSchema::name);
    if (found == schema_.fields.end())
        throw Error{ErrorCategory::validation, "unknown dataset field: " + std::string{field}};
    return static_cast<std::size_t>(std::distance(schema_.fields.begin(), found));
}

void Dataset::validate_field(std::string_view field) const {
    static_cast<void>(field_index(field));
}

void Dataset::ensure_editable() const {
    if (!is_editable())
        throw Error{ErrorCategory::validation, "this dataset is read-only"};
}

void Dataset::ensure_editing() const {
    ensure_editable();
    if (mode_ == DatasetMode::browse)
        throw Error{ErrorCategory::validation, "begin an insert or edit before changing fields"};
}

void Dataset::validate_draft() const {
    for (const auto index : modified_fields_) {
        const auto& field = schema_.fields[index];
        if (!field.nullable && (!draft_[index] || draft_[index]->is_null()))
            throw Error{ErrorCategory::validation, "field cannot be NULL: " + field.name};
    }
}

void Dataset::reset_edit_state() noexcept {
    mode_ = DatasetMode::browse;
    draft_.clear();
    modified_fields_.clear();
    original_identity_.reset();
}

void Dataset::refresh_after_write() {
    const auto count = total_count();
    if (count == 0)
        page_offset_ = 0;
    else if (page_offset_ >= count && page_offset_ != 0)
        page_offset_ = ((count - 1) / page_size_) * page_size_;
    refresh();
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
