#include "vulpes/script/runtime.hpp"

#include "vulpes/core/error.hpp"
#include "vulpes/db/value.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace vulpes::script {
namespace {

constexpr int instruction_interval = 1'000;

struct Budget {
    std::size_t memory_limit{};
    std::size_t memory_used{};
    std::size_t instructions_remaining{};
};

auto limited_allocator(void* user_data, void* allocation, std::size_t old_size, std::size_t new_size) noexcept
    -> void* {
    auto& budget = *static_cast<Budget*>(user_data);
    if (new_size == 0U) {
        std::free(allocation);
        budget.memory_used = old_size > budget.memory_used ? 0U : budget.memory_used - old_size;
        return nullptr;
    }
    if (new_size > old_size && new_size - old_size > budget.memory_limit - budget.memory_used)
        return nullptr;

    auto* replacement = std::realloc(allocation, new_size);
    if (replacement == nullptr)
        return nullptr;
    if (new_size > old_size)
        budget.memory_used += new_size - old_size;
    else
        budget.memory_used -= old_size - new_size;
    return replacement;
}

void instruction_hook(lua_State* state, lua_Debug*) {
    void* user_data = nullptr;
    static_cast<void>(lua_getallocf(state, &user_data));
    auto& budget = *static_cast<Budget*>(user_data);
    if (budget.instructions_remaining <= static_cast<std::size_t>(instruction_interval))
        luaL_error(state, "script exceeded its instruction budget");
    budget.instructions_remaining -= static_cast<std::size_t>(instruction_interval);
}

class State {
  public:
    explicit State(Limits limits)
        : budget_{.memory_limit = limits.memory_bytes, .instructions_remaining = limits.instruction_count},
          state_{lua_newstate(limited_allocator, &budget_, 0U)} {
        if (state_ == nullptr)
            throw Error{ErrorCategory::script, "could not allocate a Lua script state"};
    }

    ~State() { lua_close(state_); }

    State(const State&) = delete;
    auto operator=(const State&) -> State& = delete;

    [[nodiscard]] auto get() const noexcept -> lua_State* { return state_; }

  private:
    Budget budget_;
    lua_State* state_;
};

void open_restricted_libraries(lua_State* state) {
    luaL_requiref(state, "_G", luaopen_base, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(state, 1);

    static constexpr const char* disabled_globals[]{"collectgarbage", "dofile", "load", "loadfile", "print", "warn"};
    for (const auto* global : disabled_globals) {
        lua_pushnil(state);
        lua_setglobal(state, global);
    }
}

void push_value(lua_State* state, const db::Value& value) {
    std::visit(
        [&](const auto& stored) {
            using Stored = std::decay_t<decltype(stored)>;
            if constexpr (std::is_same_v<Stored, std::monostate> || std::is_same_v<Stored, db::Blob>)
                lua_pushnil(state);
            else if constexpr (std::is_same_v<Stored, std::int64_t>)
                lua_pushinteger(state, static_cast<lua_Integer>(stored));
            else if constexpr (std::is_same_v<Stored, double>)
                lua_pushnumber(state, static_cast<lua_Number>(stored));
            else if constexpr (std::is_same_v<Stored, std::string>)
                lua_pushlstring(state, stored.data(), stored.size());
        },
        value.storage());
}

[[nodiscard]] auto lua_value(lua_State* state, int index) -> db::Value {
    switch (lua_type(state, index)) {
    case LUA_TNIL:
        return nullptr;
    case LUA_TBOOLEAN:
        return lua_toboolean(state, index) != 0;
    case LUA_TNUMBER:
        if (lua_isinteger(state, index) != 0) {
            const auto value = lua_tointeger(state, index);
            if (value < static_cast<lua_Integer>(std::numeric_limits<std::int64_t>::min()) ||
                value > static_cast<lua_Integer>(std::numeric_limits<std::int64_t>::max())) {
                throw Error{ErrorCategory::script, "Lua integer is outside SQLite's supported range"};
            }
            return static_cast<std::int64_t>(value);
        }
        return static_cast<double>(lua_tonumber(state, index));
    case LUA_TSTRING: {
        std::size_t length{};
        const auto* text = lua_tolstring(state, index, &length);
        return std::string{text, length};
    }
    default:
        throw Error{ErrorCategory::script, "Lua record fields may contain only nil, boolean, number, or string values"};
    }
}

void push_context(lua_State* state, Hook hook, std::string_view table, std::string_view command) {
    lua_createtable(state, 0, 3);
    lua_pushlstring(state, hook_name(hook).data(), hook_name(hook).size());
    lua_setfield(state, -2, "hook");
    if (!table.empty()) {
        lua_pushlstring(state, table.data(), table.size());
        lua_setfield(state, -2, "table");
    }
    if (!command.empty()) {
        lua_pushlstring(state, command.data(), command.size());
        lua_setfield(state, -2, "command");
    }
    lua_setglobal(state, "context");
}

void push_record(lua_State* state, const model::DatasetRecord& record) {
    lua_createtable(state, 0, static_cast<int>(record.fields().size()));
    for (const auto& field : record.fields()) {
        if (!field.value || field.blob || std::holds_alternative<db::Blob>(field.value->storage()))
            continue;
        push_value(state, *field.value);
        lua_setfield(state, -2, field.name.c_str());
    }
    lua_setglobal(state, "record");
}

void synchronize_record(lua_State* state, model::DatasetRecord& record) {
    lua_getglobal(state, "record");
    if (lua_type(state, -1) != LUA_TTABLE)
        throw Error{ErrorCategory::script, "script replaced the record table"};
    const auto record_index = lua_gettop(state);

    lua_pushnil(state);
    while (lua_next(state, record_index) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING)
            throw Error{ErrorCategory::script, "script record field names must be strings"};
        std::size_t length{};
        const auto* name = lua_tolstring(state, -2, &length);
        const auto field_name = std::string_view{name, length};
        const auto* field = record.field(field_name);
        if (field == nullptr)
            throw Error{ErrorCategory::script, "script referenced an unknown record field: " + std::string{field_name}};
        if (field->blob || (field->value && std::holds_alternative<db::Blob>(field->value->storage()))) {
            throw Error{ErrorCategory::script, "BLOB fields are not available to Lua: " + std::string{field_name}};
        }
        lua_pop(state, 1);
    }

    for (const auto& field : record.fields()) {
        lua_getfield(state, record_index, field.name.c_str());
        if (field.blob || (field.value && std::holds_alternative<db::Blob>(field.value->storage()))) {
            if (lua_type(state, -1) != LUA_TNIL) {
                lua_pop(state, 1);
                throw Error{ErrorCategory::script, "BLOB fields are not available to Lua: " + field.name};
            }
            lua_pop(state, 1);
            continue;
        }

        std::optional<db::Value> replacement;
        if (lua_type(state, -1) == LUA_TNIL)
            replacement = field.value ? std::optional<db::Value>{db::Value{nullptr}} : std::nullopt;
        else
            replacement = lua_value(state, -1);
        lua_pop(state, 1);
        record.set(field.name, std::move(replacement));
    }
    lua_pop(state, 1);
}

[[nodiscard]] auto error_text(lua_State* state) -> std::string {
    const auto* message = lua_tostring(state, -1);
    return message == nullptr ? "Lua returned a non-text error" : std::string{message};
}

[[nodiscard]] auto matching(const Definition& definition, Hook hook, std::string_view table, std::string_view command)
    -> bool {
    return definition.hook == hook && (!definition.table || *definition.table == table) &&
           (!definition.command || *definition.command == command);
}

} // namespace

Runtime::Runtime(std::vector<Definition> definitions, Limits limits)
    : definitions_{std::move(definitions)}, limits_{limits} {
    if (limits_.memory_bytes == 0U || limits_.instruction_count == 0U)
        throw Error{ErrorCategory::script, "Lua script limits must be positive"};
    std::ranges::sort(definitions_, {},
                      [](const auto& definition) { return std::pair{definition.position, definition.name}; });
}

void Runtime::on_open() const {
    invoke(Hook::on_open, nullptr, {});
}

void Runtime::on_command(std::string_view command) const {
    invoke(Hook::on_command, nullptr, command);
}

void Runtime::invoke(model::DatasetHook hook, model::DatasetRecord& record) {
    switch (hook) {
    case model::DatasetHook::before_insert:
        invoke(Hook::before_insert, &record, {});
        return;
    case model::DatasetHook::before_update:
        invoke(Hook::before_update, &record, {});
        return;
    case model::DatasetHook::after_update:
        invoke(Hook::after_update, &record, {});
        return;
    case model::DatasetHook::before_delete:
        invoke(Hook::before_delete, &record, {});
        return;
    }
}

void Runtime::invoke(Hook hook, model::DatasetRecord* record, std::string_view command) const {
    const auto table = record == nullptr ? std::string_view{} : record->table();
    for (const auto& definition : definitions_) {
        if (!matching(definition, hook, table, command))
            continue;

        State state{limits_};
        auto* lua = state.get();
        open_restricted_libraries(lua);
        push_context(lua, hook, table, command);
        if (record != nullptr)
            push_record(lua, *record);
        lua_sethook(lua, instruction_hook, LUA_MASKCOUNT, instruction_interval);
        if (luaL_loadbufferx(lua, definition.source.data(), definition.source.size(), definition.name.c_str(), "t") !=
            LUA_OK) {
            throw Error{ErrorCategory::script,
                        "Lua script '" + definition.name + "' failed to load: " + error_text(lua)};
        }
        if (lua_pcall(lua, 0, 0, 0) != LUA_OK) {
            throw Error{ErrorCategory::script, "Lua script '" + definition.name + "' failed: " + error_text(lua)};
        }
        if (record != nullptr)
            synchronize_record(lua, *record);
    }
}

} // namespace vulpes::script
