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

Development versions are derived from Git, for example
`0.1.0-dev+gabcdef123456` (with `.dirty` when configured from a modified tree).
An exact `vMAJOR.MINOR.PATCH` tag produces that release version. Source-archive
packagers can set `-DVULPES_BUILD_VERSION_OVERRIDE=<version>` when Git metadata
is intentionally unavailable.

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
`Ctrl+C` to exit. `Ctrl+R` and the File/Database menus open an existing database
read-only; only **Create database** uses SQLite's create mode. `Alt+D`, `Alt+V`,
`Alt+W`, and `Alt+H` open the other menu groups. Browse and SQL tabs host their full interactive surfaces; `Ctrl+Tab`
switches tabs and `Ctrl+W` asks before closing the active non-workspace tab.
Opening or creating another database closes its old documents, so no tab can
retain a dataset from the previous database.

Menus use measured Unicode window chrome, a right-aligned shortcut column, and
visually disabled actions when their prerequisites are unavailable. Type the
underlined first letter of a unique menu item to invoke it; repeated first
letters cycle among the matching items. Pop-up windows are opaque and clipped
to the current terminal, so closing a menu or dialog restores the document
beneath it on the next diffed frame.

Choose **Browse files...** from the File or Database menu to navigate the
current directory without leaving Vulpes. Enter opens a directory or selects a
file, Backspace goes to its parent, Home/End move within the listing, and Esc
cancels. The browser is optional: manual path entry remains available for local,
network, and known paths.

The workspace keeps the ten most recently opened or created databases in a
small, versioned user settings file. The home screen lists them; use Up/Down
and Enter to reopen a selected entry. The default settings path is
`%APPDATA%\Vulpes\settings.json` on Windows,
`~/Library/Application Support/Vulpes/settings.json` on macOS, and
`$XDG_CONFIG_HOME/vulpes/settings.json` (or `~/.config/vulpes/settings.json`)
on Linux. Use `--config path\to\settings.json` for a portable or test-specific
location. This file contains only local workspace state, never application or
SQLite database metadata.

Press `Ctrl+P` to open the command palette. It uses the same parser and
runtime as `--command`: `help`, `tables`, `schema <table>`, `browse <table>`,
`sql`, and `quit` are supported. `schema <table>` opens a read-only schema tab;
`browse` and `sql` open their persistent workspace documents.

With a translated catalog, menu Alt mnemonics follow the first character of
each translated menu label; `F10` always opens the first menu. For example, the
shipped Czech catalog uses `Alt+S` for `Soubor`.

The default palette is `midnight`. It is applied consistently to the workspace,
grids, forms, prompts, SQL console, schema documents, and window chrome. Use
`--theme high-contrast` for a black-and-white high-contrast presentation with
underlined mnemonics; the option also applies to direct `--table` and `--sql`
sessions:

```powershell
.\build\windows-msvc\Debug\vulpes.exe --theme high-contrast
```

To verify a terminal host after an input or rendering change, run
`vulpes --terminal-diagnostics`. It shows Vulpes' normalized `Key` and `Resize`
events; test arrow keys, Escape (exits), Ctrl+C (exits), function keys, and Alt
chords. It deliberately exposes Vulpes events rather than CPP-Terminal details,
so a report is useful across Windows, Linux, and macOS hosts.

Run `vulpes --terminal-capabilities` to inspect standard-input and
standard-output availability without entering raw mode. Interactive commands
reject redirected streams before initializing the full-screen backend and emit
a plain error with no terminal control sequences. Non-interactive commands such
as `--version` and schema listing remain safe to redirect.

The browse view is keyboard driven: arrow keys move through the grid, `F2`
opens the selected record, `Insert` creates a record, `F8` saves a record form,
and `Esc` cancels it. Form controls are inferred from SQLite schema information:
numeric fields use numeric validation, conservative boolean-like fields use a
checkbox, generated/primary-key fields are protected, and BLOB fields remain
read-only until a binary editor exists. Compact form windows keep the focused
field visible and show scroll indicators when more fields exist. Failed database
validation leaves the draft open for correction. Grids size columns from their
headers and current page rather than assigning every column the same width; the
focused cell remains distinct from the selected row.

Single-line prompts and generated text/number fields have a logical UTF-8
cursor. Use Left/Right, Home/End, Backspace, and Delete to edit in place. Long
values follow the caret horizontally without changing the stored text. Lookup
fields retain Left/Right for relationship selection. Selection, clipboard, and
word-wise movement remain tracked follow-up work.

Read-only database sessions keep browse, sort, filter, search, schema, and SQL
result viewing available, but generated form and delete actions are disabled and
the browse footer is labeled **read-only**. An explicit database path supplied
on the command line is opened read/write without create permission, so a typo
cannot silently create a new SQLite file.

Browse keys are mapped to stable semantic actions (for example,
`record.edit` and `dataset.refresh`) before they reach application controllers.
The shipped mapping can be replaced programmatically; persistent user keybinding
configuration remains deferred.

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
cursor-aware multiline input: arrows move by character or line, Home/End move
within a line, PageUp/PageDown move by an editor page, Enter splits a line, Tab
indents to a four-column stop, and Backspace/Delete join lines at their edges.
The editor follows the caret vertically and horizontally; border arrows mark
hidden content. `F8` executes and presents the final row-producing statement
through the same Grid widget as `browse`. After results appear, `F7` switches
keyboard focus between the editor and result Grid; `Esc` first returns focus to
the editor and then returns to the shell. Execution is bounded to 1,000 result
rows by default, and the console reports truncation and affected-row counts.
SQL history, parameter prompts, and multiple displayed result sets remain
deliberately deferred.

`--command` accepts `help`, `tables`, `schema <table>`, `browse <table>`,
`sql`, and `quit`. It is a non-interactive bridge to the same application
command dispatcher used by the in-app command palette.

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
