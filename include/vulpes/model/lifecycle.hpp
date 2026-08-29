#pragma once

#include "vulpes/db/value.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vulpes::model {

// Dataset hooks are semantic extension points. They deliberately contain owned
// data only, so an implementation cannot depend on a SQLite handle or a UI.
enum class DatasetHook { before_insert, before_update, after_update, before_delete };

struct DatasetRecordField {
    std::string name;
    std::optional<db::Value> value;
    bool writable{false};
    bool blob{false};
};

class DatasetRecord {
  public:
    DatasetRecord(std::string table, std::vector<DatasetRecordField> fields);

    [[nodiscard]] auto table() const noexcept -> std::string_view { return table_; }
    [[nodiscard]] auto fields() const noexcept -> const std::vector<DatasetRecordField>& { return fields_; }
    [[nodiscard]] auto field(std::string_view name) const noexcept -> const DatasetRecordField*;
    [[nodiscard]] auto field(std::string_view name) noexcept -> DatasetRecordField*;
    void set(std::string_view name, std::optional<db::Value> value);

  private:
    std::string table_;
    std::vector<DatasetRecordField> fields_;
};

class DatasetLifecycle {
  public:
    virtual ~DatasetLifecycle() = default;

    virtual void invoke(DatasetHook hook, DatasetRecord& record) = 0;
};

[[nodiscard]] auto dataset_hook_name(DatasetHook hook) noexcept -> std::string_view;

} // namespace vulpes::model
