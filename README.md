# Vulpes

Vulpes is a local-first, keyboard-driven RAD environment that turns an ordinary
SQLite schema into an application. The first frontend is a TUI; its database,
application, semantic UI, and rendering layers are deliberately independent.

The repository is pre-alpha. It currently provides a C++23 build, RAII SQLite
primitives, schema introspection, a CPP-Terminal-backed input layer, virtual
screen rendering, a keyboard-driven workspace, browse/edit views, and
schema-generated record forms. The workspace is still being expanded into a
full document host.

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
SQLite and Catch2 automatically. CPP-Terminal is fetched at a reviewed, pinned
commit by CMake because it is not present in the pinned vcpkg registry. The
`sqlite3` CLI is optional and is used only by the example-database command below.

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

To try the workspace without a database, run:

```powershell
.\build\windows-msvc\Debug\vulpes.exe
```

Use `Ctrl+O` to open a SQLite file, `Ctrl+N` to create one, `F10` (or `Alt+F`)
for the File menu, arrow keys to navigate, `Esc` to close a menu or dialog, and
`Ctrl+C` to exit. `Alt+D`, `Alt+V`, `Alt+W`, and `Alt+H` open the other menu
groups. Browse and SQL tabs host their full interactive surfaces; `Ctrl+Tab`
switches tabs and `Ctrl+W` closes the active non-workspace tab.

The default workspace palette is `midnight`. Use `--theme high-contrast` for a
black-and-white high-contrast workspace chrome with underlined mnemonics:

```powershell
.\build\windows-msvc\Debug\vulpes.exe --theme high-contrast
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

Open the interactive SQL console with `--sql` or `--command sql`. It accepts
multiline input (`Enter` adds a line; `F8` executes) and presents the final
row-producing statement through the same Grid widget as `browse`. Execution is
bounded to 1,000 result rows by default; the console reports truncation and
affected-row counts. `Esc` returns to the shell. SQL history, parameter prompts,
and multiple displayed result sets remain deliberately deferred.

`--command` currently accepts `help`, `tables`, `schema <table>`, `browse
<table>`, `sql`, and `quit`. It is a non-interactive bridge to the same application
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
