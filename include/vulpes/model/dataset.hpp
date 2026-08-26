#pragma once

#include "vulpes/db/row.hpp"
#include "vulpes/db/schema.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vulpes::db {
class Database;
}

namespace vulpes::model {

enum class SortDirection { ascending, descending };
enum class FilterOperator { equal, not_equal, less, less_equal, greater, greater_equal };
enum class DatasetMode { browse, insert, edit };

struct Filter {
    std::string field;
    FilterOperator comparison{FilterOperator::equal};
    db::Value value;
};

struct RowIdentity {
    std::vector<std::string> fields;
    std::vector<db::Value> values;
};

struct LookupOption {
    db::Value value;
    std::string label;
};

// A table-backed cursor model. It owns query and editing state, so UI code never
// depends on SQLite statement lifetime or assembles raw SQL.
class Dataset {
  public:
    Dataset(db::Database& database, db::TableSchema schema, std::size_t page_size = 100);

    [[nodiscard]] auto schema() const noexcept -> const db::TableSchema& { return schema_; }
    [[nodiscard]] auto rows() const noexcept -> const std::vector<db::Row>& { return rows_; }
    [[nodiscard]] auto total_count() -> std::size_t;
    [[nodiscard]] auto current() const -> std::optional<db::Row>;
    [[nodiscard]] auto current_row_index() const -> std::optional<std::size_t>;
    [[nodiscard]] auto current_identity() const -> std::optional<RowIdentity>;
    [[nodiscard]] auto is_editable() const noexcept -> bool;
    [[nodiscard]] auto mode() const noexcept -> DatasetMode { return mode_; }
    [[nodiscard]] auto is_dirty() const noexcept -> bool { return !modified_fields_.empty(); }
    [[nodiscard]] auto page_size() const noexcept -> std::size_t { return page_size_; }
    [[nodiscard]] auto page_offset() const noexcept -> std::size_t { return page_offset_; }

    auto order_by(std::string_view field, SortDirection direction = SortDirection::ascending) -> Dataset&;
    auto where(Filter filter) -> Dataset&;
    auto search(std::string_view text) -> Dataset&;
    auto clear_filters() -> Dataset&;
    auto clear_search() -> Dataset&;

    void begin_insert();
    void begin_edit();
    auto set(std::string_view field, db::Value value) -> Dataset&;
    [[nodiscard]] auto draft_value(std::string_view field) const -> std::optional<db::Value>;
    [[nodiscard]] auto lookup_options(std::string_view field, std::size_t limit = 100) const
        -> std::vector<LookupOption>;
    void save();
    void cancel() noexcept;
    void erase();

    void refresh();
    [[nodiscard]] auto first() -> bool;
    [[nodiscard]] auto next() -> bool;
    [[nodiscard]] auto previous() -> bool;
    [[nodiscard]] auto last() -> bool;

  private:
    [[nodiscard]] auto field_index(std::string_view field) const -> std::size_t;
    void validate_field(std::string_view field) const;
    void ensure_editable() const;
    void ensure_editing() const;
    void validate_draft() const;
    void reset_edit_state() noexcept;
    void refresh_after_write();
    [[nodiscard]] auto query_where_clause(std::vector<db::Value>& values) const -> std::string;
    [[nodiscard]] auto order_clause() const -> std::string;
    [[nodiscard]] auto has_text_fields() const -> bool;
    [[nodiscard]] auto foreign_key_for(std::string_view field) const -> const db::ForeignKeySchema*;

    db::Database* database_;
    db::TableSchema schema_;
    std::size_t page_size_;
    std::size_t page_offset_{};
    std::size_t current_index_{};
    std::optional<std::pair<std::string, SortDirection>> order_;
    std::vector<Filter> filters_;
    std::optional<std::string> search_;
    std::vector<db::Row> rows_;
    DatasetMode mode_{DatasetMode::browse};
    std::vector<std::optional<db::Value>> draft_;
    std::vector<std::size_t> modified_fields_;
    std::optional<RowIdentity> original_identity_;
};

} // namespace vulpes::model
