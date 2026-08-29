# ADR 0030: Lua business-logic hooks

- Status: accepted
- Date: 2026-08-29

## Context

SQLite schema and declarative application metadata make a generated Vulpes
application useful immediately, but practical applications also need small,
local business rules. Those rules must not give forms raw SQLite handles,
couple an application to terminal rendering, or turn Vulpes into a new
programming-language platform.

## Decision

Vulpes embeds Lua 5.5 through the pinned vcpkg `lua` package. The Lua C API is
private to `script::Runtime`; public Vulpes headers expose no `lua_State`, Lua
value, or terminal type.

Metadata schema version 3 adds `_app_scripts`. Each script has a stable name,
hook, optional table or command scope, source, and deterministic
`position`/name ordering. Validated scripts become owned `script::Definition`
values in `ApplicationDefinition`.

Each invocation gets a fresh Lua state with a 1 MiB allocation budget and a
100,000-instruction budget. Only base language helpers plus the `math`,
`string`, `table`, and `utf8` libraries are opened. File, process, module,
debug, dynamic-loading, and dynamic-code-loading facilities are unavailable.
There is no database, terminal, filesystem, network, process, or native C++
object in the Lua environment.

Record hooks receive a plain `record` table and a `context` table. Values may
be nil, boolean, number, or string. Unknown fields, changed non-writable
fields, and BLOB access are rejected when the script returns. Before-insert and
before-update hooks may change writable record fields; after-update and
before-delete records are read-only. `on_open` and `on_command` receive only
context. A script calls the standard Lua `error` function to reject an action.

`model::DatasetLifecycle` is the UI-neutral extension boundary. The dataset
opens a transaction before record hooks, applies the write, then runs
`after_update` before committing. A hook failure rolls back the transaction and
leaves a form draft available for correction. `ApplicationRuntime` invokes an
`on_command` hook once for the submitted top-level semantic command before
dispatching it. `on_open` runs only when a metadata-defined application opens,
not for a standalone `--query` export.

## Consequences

- Lua can validate and normalize data without exposing raw SQLite or widgets.
- Hook semantics remain reusable by future GUI/web frontends and native
  extensions through `DatasetLifecycle`.
- Script errors are structured `ErrorCategory::script` errors; callers keep
  presentation and localization responsibilities at their boundaries.
- Resource limits limit accidental runaway scripts but do **not** make a
  `.vulpes` file from an untrusted source safe to open. Application scripts are
  trusted code and need the same review as application SQL or native extensions.
- Fresh interpreter states intentionally prohibit module imports, cross-hook
  global state, and a script-side persistence model. New capabilities require a
  reviewed host-API addition and ADR.
