# Vulpes

Vulpes is a local-first, keyboard-driven RAD environment that turns an ordinary
SQLite schema into an application. The first frontend is a TUI; its database,
application, semantic UI, and rendering layers are deliberately independent.

The repository is pre-alpha. It currently provides a C++23 build,
cross-platform CI, RAII SQLite primitives, schema introspection, normalized
input, a virtual screen buffer, a keyboard-driven `--table` browse view, and a
transactional dataset editing model, and schema-generated record forms. The
in-app command window is the next frontend milestone.

## Prerequisites

- CMake 3.31 or newer
- Ninja 1.12 or newer
- Git
- vcpkg, with `VCPKG_ROOT` set to its absolute installation directory
- A current compiler: MSVC 18, GCC 14+, or Clang/AppleClang 18+

On Windows, run commands from an **x64 Native Tools Command Prompt for VS 2026**
so `cl.exe` is available. Visual Studio 2026 includes vcpkg; on a default
Community installation it can be selected for the current shell with:

```powershell
$env:VCPKG_ROOT = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg'
```

Alternatively, clone and bootstrap vcpkg separately. CMake manifest mode installs
SQLite and Catch2 automatically. The `sqlite3` CLI is optional and is used only
by the example-database command below.

## Build and test on Windows

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug
ctest --preset windows-debug
.\build\windows-msvc\Debug\vulpes.exe --version
```

To exercise the first schema-listing vertical slice after creating a database:

```powershell
sqlite3 inventory.db ".read examples/inventory/schema.sql"
sqlite3 inventory.db ".read examples/inventory/seed.sql"
.\build\windows-msvc\Debug\vulpes.exe inventory.db
.\build\windows-msvc\Debug\vulpes.exe inventory.db --command "schema products"
.\build\windows-msvc\Debug\vulpes.exe inventory.db --table products
```

The browse view is keyboard driven: arrow keys move through the grid, `F2`
opens the selected record, `Insert` creates a record, `F8` saves a record form,
and `Esc` cancels it. Form controls are inferred from SQLite schema information:
numeric fields use numeric validation, conservative boolean-like fields use a
checkbox, generated/primary-key fields are protected, and BLOB fields remain
read-only until a binary editor exists. Failed database validation leaves the
draft open for correction.

Browse keys are mapped to stable semantic actions (for example,
`record.edit` and `dataset.refresh`) before they reach application controllers.
The shipped mapping can be replaced programmatically; persistent user keybinding
configuration will arrive with the configuration layer.

Within a browse view, `F3` opens a text search over text columns, `F4` filters
the selected column, `F5` refreshes, and `F6` sorts the selected column
(repeating it reverses the direction). Filter values are parsed according to
the selected field's declared numeric type, accept `=`, `!=`, `<>`, `<`, `<=`,
`>`, and `>=` prefixes, and accept `NULL` for null comparisons. A blank search
or filter input clears its respective constraint. Values are still bound as
SQLite parameters; the TUI never interpolates user input into SQL.

`Delete` opens a confirmation dialog that defaults to **Cancel**; choose Delete
with Left/Right or `Y`, then press Enter. Deletion uses the same transactional,
primary-key-guarded dataset operation as other writes.

Foreign-key fields in generated forms are rendered as lookups rather than raw
keys. Vulpes infers a related display field in this order: `name`, `title`,
`description`, then `code`. Use Left/Right on the lookup to choose from the
first 100 related rows; Vulpes stores the underlying key. Searchable and
metadata-configured lookups are intentionally deferred.

The database layer also provides a bounded SQL execution model for the upcoming
SQL console: it executes a script, reports affected rows, and retains owned
rows from its final row-producing statement (up to 1,000 by default). The
interactive console and grid adapter are the next step.

`--command` currently accepts `help`, `tables`, `schema <table>`, `browse
<table>`, and `quit`. It is a non-interactive bridge to the same application
command dispatcher that will power the in-app command window.

Interface messages use BCP-47 locales and optional UTF-8 JSON catalogs. For
example, use the shipped Czech translation with `--locale cs-CZ --catalog
translations\cs.json`. See [docs/localization.md](docs/localization.md).

The inventory example's seed data and acceptance test exercise only generic
Vulpes capabilities: schema-driven datasets, transactional editing, stock
movement insertion, and SQL views. It contains no runtime special cases.

See [docs/architecture.md](docs/architecture.md), [docs/development.md](docs/development.md),
and [TODO.md](TODO.md) before changing subsystem boundaries or choosing work.

## Project status

Version 0.1 is pre-alpha. APIs are expected to evolve; the data-modifying UI is
currently limited to basic generated forms. Database backups remain the user's
responsibility.

## License

MIT. See [LICENSE](LICENSE).
