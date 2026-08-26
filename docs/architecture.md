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

## Initial decisions

### C++23 with a conservative feature surface

C++23 is the language mode because current supported compilers implement it and
MSVC 18 is the primary local toolchain. Public APIs initially use broadly
implemented vocabulary types so platform support remains practical.

### vcpkg manifest dependencies

The checked-in registry baseline makes dependency resolution reproducible.
SQLite and Catch2 are the only dependencies. Terminal and Unicode libraries are
deferred until an implementation spike compares behavior on all three platforms.

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
Current bounded OFFSET paging is deliberately behind this boundary; keyset
paging is the next optimization when a stable unique ordering is available.
Dataset writes are transactional and schema-validated; see ADR 0001 and ADR
0005.

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

## Deferred decisions

- Terminal library versus small native ANSI/Windows backends.
- Unicode segmentation and display-width library.
- Dataset public API and concurrency model.
- Metadata table format.
- Lua runtime and sandbox policy.

Each requires a focused cross-platform spike and an ADR before adoption.
