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
is initially five seconds and will become configuration.

### Data paging

The dataset layer will own paging and row identity. Widgets will request logical
rows and never assemble SQL. Keyset paging is preferred when a stable unique
ordering exists; a bounded OFFSET fallback is acceptable otherwise.

### Terminal rendering

Widgets render semantic cells into `ScreenBuffer`. A backend diffs frames and
encodes cells for the host. Width is not equivalent to code-point count, so a
Unicode-width dependency must be selected before text layout is implemented.

## Deferred decisions

- Terminal library versus small native ANSI/Windows backends.
- Unicode segmentation and display-width library.
- Dataset public API and concurrency model.
- Metadata table format.
- Lua runtime and sandbox policy.

Each requires a focused cross-platform spike and an ADR before adoption.

