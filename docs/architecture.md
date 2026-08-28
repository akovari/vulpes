# Architecture

## Direction

Dependencies point inward and downward:

```text
frontend (TUI; GUI/web later)
  -> semantic UI and application runtime
    -> dataset/cursor model
      -> db abstractions
        -> SQLite C API

terminal backend -> virtual screen and normalized input
widgets          -> virtual screen and normalized input
```

The application runtime must not include terminal backend headers. Widgets must
not include SQLite headers. Only `src/db` may include `sqlite3.h`; public headers
forward-declare opaque SQLite types where ownership requires it.

## Source layout

- `include/vulpes/core`: shared errors and application-level primitives.
- `ApplicationRuntime` in `core` translates parsed semantic commands into data
  results; it has no terminal or widget dependency.
- `include/vulpes/db`, `src/db`: SQLite ownership, values, statements,
  transactions, and UI-independent schema discovery.
- `include/vulpes/model`, `src/model`: datasets, fields, relationships, and
  validation (Phase 2; created when behavior is implemented).
- `include/vulpes/ui`, `src/ui`: semantic widgets (Phase 3).
- `include/vulpes/terminal`, `src/terminal`: virtual screen, normalized input,
  and platform backends.
- `include/vulpes/appmeta`, `src/appmeta`: optional metadata (post-0.1).
- `examples/inventory`: generic framework dogfood; never a source of inventory
  special cases in the runtime.

Directories are added when they contain working code. This avoids empty modules
that imply stability or design decisions not yet earned.

Workspace document surfaces own UI-level state such as a dataset/grid/form or
SQL editor/result grid. The workspace shell owns tabs and routes normalized
events to the active surface. Neither layer knows terminal escape sequences;
database operations remain inside `Dataset` or the explicit SQL-console
boundary.

`DocumentSession` is the direct-mode terminal host for a single
`DocumentSurface`. Consequently `vulpes database.db --table customers` and the
workspace Browse command run the same browse/form/filter implementation; the
same is true of direct and workspace SQL consoles. It also gives reduced
terminal sizes a deterministic warning frame and preserves Escape/Ctrl+C as
clean exits while waiting for a resize.

## Initial decisions

### C++23 with a conservative feature surface

C++23 is the language mode because current supported compilers implement it and
MSVC 18 is the primary local toolchain. Public APIs initially use broadly
implemented vocabulary types so platform support remains practical.

### vcpkg manifest dependencies

The checked-in registry baseline makes dependency resolution reproducible.
SQLite, Catch2, utf8proc, CLI11, and nlohmann/json each have a narrow boundary:
database access, tests, Unicode cell handling, process argument parsing, and
external message-catalog parsing respectively. CPP-Terminal is fetched at a
pinned commit because it is unavailable in the pinned vcpkg registry; it owns
host-terminal raw mode and event transport behind `terminal::ConsoleTerminal`.
Terminal rendering remains behind Vulpes abstractions rather than a
framework-specific widget model.

### SQLite threading and ownership

One `Database` owns one connection and is movable, not copyable. Statements are
movable, not copyable, and cannot outlive their database. This lifetime rule will
be made mechanically explicit if asynchronous execution is later introduced.
Foreign keys and extended result codes are enabled per connection; busy timeout
is initially five seconds and will become configuration. SQLite TEXT values are
kept as raw byte strings by the database layer, including invalid UTF-8, to
preserve SQLite semantics. The terminal boundary is responsible for safe display
of externally supplied text.

### Dataset paging and editing

The dataset layer owns paging, row identity, filters, ordering, editing state,
and owning row snapshots. Widgets request logical rows and never assemble SQL.
Stable single-column primary-key and non-null unique-order datasets use keyset
paging; composite, nullable, and non-unique orderings retain bounded OFFSET
pages. The strategy remains behind the dataset boundary.
Dataset writes are transactional and schema-validated; see ADR 0001 and ADR
0005.

### Generated record forms

`ui::RecordForm` turns a dataset draft and schema fields into semantic controls.
It has no SQLite dependency: it invokes only dataset edit operations and renders
only to a `ScreenBuffer`. Form field labels currently default to schema names;
metadata and localized labels will be injected rather than baked into layout.
The initial inference is intentionally conservative: binary and generated data
are read-only, numeric declarations get numeric parsing, and boolean hints are
limited to explicit types and well-known field names. See ADR 0006.

On a failed save, the form preserves the dataset draft and maps a validation or
constraint error to the named schema field when SQLite supplies one. It selects
and marks that field; ambiguous failures are attributed only when exactly one
editable field changed.

### Destructive confirmation

`ui::ConfirmationDialog` is a reusable semantic dialog whose default selection
is cancel. It has no database dependency and receives all visible strings from
the presentation caller. Browse uses it before delegating deletion to
`Dataset::erase`; the dataset remains responsible for transactional execution
and stable primary-key identity. See ADR 0008.

### Relationship lookups

`Dataset::lookup_options` discovers a field's foreign-key schema, resolves a
small, schema-derived display-field heuristic, and returns bound key/label
pairs. `RecordForm` renders the label but saves the key. The option list is
bounded to 100 rows and does not expose database handles or SQL to the widget;
searchable and metadata-defined lookups remain a later model feature. See ADR
0009.

### Commands

The command parser produces stable `CommandId` values. `ApplicationRuntime`
validates command arity and resolves schema objects, then returns a semantic
`CommandResponse` for the caller to localize and render. The executable's
`--command` option is a non-interactive adapter; a later command-line widget
will use the same runtime rather than execute SQLite directly.

### Terminal rendering

Widgets render semantic cells into `ScreenBuffer`. A backend diffs frames and
encodes cells for the host. utf8proc supplies current Unicode cell-width data and
UTF-8 decoding. Extended grapheme layout is deliberately deferred; see ADR 0002.
`ConsoleTerminal` is the current native adapter and is replaceable; see ADR 0004.

### Browse query controls

The browse frontend uses a semantic `TextPrompt` widget for text search and
field filters. The widget owns text editing only; the frontend parses a small
comparison prefix grammar and calls `Dataset::search`, `where`, `order_by`, or
`refresh`. This keeps SQLite identifiers and values parameterized through the
dataset model. User-facing prompt and footer text comes from the `Localizer`,
so adding a translation does not alter commands, actions, or database names.

### Semantic actions and key bindings

`core::ActionMap` maps normalized `KeyEvent` values to stable `ActionId`
values such as `record.edit` and `dataset.refresh`. Browse controllers receive
only actions, so neither Windows virtual keys nor ANSI sequences become part of
application behavior. The built-in mapping supplies the documented browse keys;
callers can replace individual bindings through `ActionMap::bind`. Persisting
user-configured bindings is deliberately deferred until the configuration
format exists.

### SQL console boundary

`Database::run_sql` is the sole arbitrary-SQL boundary. It keeps the SQLite C
API internal, executes a complete script, and returns an owned `SqlResult` for
the final statement with columns. Result rows are bounded to protect terminal
renderers from accidental unbounded queries. `ui::SqlConsole` owns multiline
editing only; the frontend performs execution, converts an owned `SqlResult` to
`ui::GridRows`, and reuses `Grid` for result rendering. Neither widget performs
SQLite work.

## Deferred decisions

- Terminal library versus small native ANSI/Windows backends.
- Unicode segmentation and display-width library.
- Dataset public API and concurrency model.
- Metadata table format.
- Lua runtime and sandbox policy.

Each requires a focused cross-platform spike and an ADR before adoption.
