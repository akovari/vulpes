#include "vulpes/model/lifecycle.hpp"

#include "vulpes/core/error.hpp"

#include <algorithm>

namespace vulpes::model {

DatasetRecord::DatasetRecord(std::string table, std::vector<DatasetRecordField> fields)
    : table_{std::move(table)}, fields_{std::move(fields)} {
}

auto DatasetRecord::field(std::string_view name) const noexcept -> const DatasetRecordField* {
    const auto found = std::ranges::find(fields_, name, &DatasetRecordField::name);
    return found == fields_.end() ? nullptr : &*found;
}

auto DatasetRecord::field(std::string_view name) noexcept -> DatasetRecordField* {
    const auto found = std::ranges::find(fields_, name, &DatasetRecordField::name);
    return found == fields_.end() ? nullptr : &*found;
}

void DatasetRecord::set(std::string_view name, std::optional<db::Value> value) {
    auto* destination = field(name);
    if (destination == nullptr)
        throw Error{ErrorCategory::validation, "unknown record field: " + std::string{name}};
    if (!destination->writable && destination->value != value)
        throw Error{ErrorCategory::validation, "record field is read-only: " + std::string{name}};
    destination->value = std::move(value);
}

auto dataset_hook_name(DatasetHook hook) noexcept -> std::string_view {
    switch (hook) {
    case DatasetHook::before_insert:
        return "before_insert";
    case DatasetHook::before_update:
        return "before_update";
    case DatasetHook::after_update:
        return "after_update";
    case DatasetHook::before_delete:
        return "before_delete";
    }
    return {};
}

} // namespace vulpes::model
