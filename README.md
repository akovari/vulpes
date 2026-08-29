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
SQLite, Catch2, utf8proc, CLI11, nlohmann/json, dacap/clip, ICU, and zlib
automatically. CPP-Terminal and PDFio are fetched at reviewed, pinned commits by
CMake because they are not present in the pinned vcpkg registry. PDF export
embeds a Unicode-capable font and does not require a system font installation.
The `sqlite3` CLI is optional and is used only by the example-database command
below.

## Build and test on Windows

```powershell
.\scripts\dev.ps1
.\build\windows-msvc\Debug\vulpes.exe --version
```

The script discovers the newest Visual Studio installation, initializes its x64
MSVC environment, and selects bundled vcpkg and LLVM tools when the corresponding
environment variables are not already set. Its default `check` task reuses or
creates the Debug configuration, builds incrementally, checks formatting, and
runs tests. Use `configure` to refresh CMake explicitly, `build`
for a compile-only iteration, `test -CTestRegex <pattern>` for focused CTest,
`format` to apply clang-format, and `tidy` for the separate static-analysis build.
Release compilation is intentionally explicit: run `.\scripts\dev.ps1 release`
only for a milestone or release gate. All underlying CMake presets remain usable
directly, including on Linux and macOS.

Development versions are derived from Git, for example
`0.1.0-dev+gabcdef123456` (with `.dirty` when configured from a modified tree).
An exact `vMAJOR.MINOR.PATCH` tag produces that release version. Source-archive
packagers can set `-DVULPES_BUILD_VERSION_OVERRIDE=<version>` when Git metadata
is intentionally unavailable.

Release archives are explicit, tag-gated operations: `.\scripts\dev.ps1 package`
creates a Release ZIP plus SHA-256 sidecar after running Release tests from a clean
`vMAJOR.MINOR.PATCH` tag. See [docs/releasing.md](docs/releasing.md) for the
artifact contract, [docs/release-notes.md](docs/release-notes.md) for the
current release status, and [docs/operations.md](docs/operations.md) for
backup, recovery, compatibility, and upgrade guidance.

To open the workspace on an existing database, or invoke one direct command:

```powershell
sqlite3 inventory.db ".read examples/inventory/schema.sql"
sqlite3 inventory.db ".read examples/inventory/seed.sql"
.\build\windows-msvc\Debug\vulpes.exe inventory.db
.\build\windows-msvc\Debug\vulpes.exe inventory.db --command tables
.\build\windows-msvc\Debug\vulpes.exe inventory.db --command "schema products"
.\build\windows-msvc\Debug\vulpes.exe inventory.db --table products
```

The smaller workshop acceptance database from the product definition can be
created in the same way from `examples/workshop/schema.sql` and
`examples/workshop/seed.sql`. See [examples/README.md](examples/README.md) for
the workflows each example is expected to exercise.

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
visually disabled actions when their prerequisites are unavailable. Type a
menu item's underlined mnemonic to invoke it. Pop-up windows are opaque and clipped
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

Menu Alt mnemonics are explicit catalog entries rather than inferred from label
text; `F10` always opens the first menu. Each mnemonic must occur in its label
and be unique within its menu scope or catalog initialization fails with a
metadata error. The shipped Czech catalog deliberately uses `Alt+S` for
`Soubor`, while item mnemonics may highlight a later character such as `J` in
`Otevřít jen pro čtení`.

The default palette is `midnight`. It is applied consistently to the workspace,
grids, forms, prompts, SQL console, schema documents, and window chrome. Use
`--theme high-contrast` for a black-and-white high-contrast presentation with
underlined mnemonics; the option also applies to direct `--table` and `--sql`
sessions:

```powershell
.\build\windows-msvc\Debug\vulpes.exe --theme high-contrast
```

These surfaces have a deterministic rendering matrix at 40x10, 80x25, and
160x45 terminal cells for English and Czech under both palettes. Vulpes uses
terminal cells rather than pixels, so Windows display scaling changes the
host's cell size, not application layout coordinates.

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
validation leaves the draft open for correction. Enter on a relationship opens
a searchable lookup: type to filter, use Up/Down, press F2 to inspect the related
row, Enter to apply, and Esc to unwind one window. Grids size columns from their
headers and current page rather than assigning every column the same width; the
focused cell remains distinct from the selected row. The footer reports the
absolute row and selected-column position, border markers expose horizontal and
vertical overflow, and empty datasets have an explicit empty state. Use
`Ctrl+Left` and `Ctrl+Right` to narrow or widen the selected column for the
current document.

The UI-neutral application-metadata model can override generated table/field
labels, field order and visibility, additional read-only policy, display formats,
and relationship behavior without terminal coordinates. Explicit TEXT
annotations provide strict ISO date/time editing and locale-aware presentation;
date-times require an RFC 3339 offset and a display time zone. SQLite-resident
metadata loading uses explicit, versioned `_app_*` tables, so ordinary databases
continue to use safe schema inference without being modified. Initialize an
existing database with `--migrate-app`; see
[docs/application-metadata.md](docs/application-metadata.md) for the schema,
commands, app-mode launch behavior, and inventory definition.

Single-line prompts and generated text/number fields have a logical UTF-8
cursor. Use Left/Right, Home/End, Backspace, and Delete to edit in place. Long
values follow the caret horizontally without changing the stored text. Lookup
fields retain Left/Right for quick relationship cycling. Hold Shift while moving to
select, use Ctrl+Left/Right for Unicode-aware word movement, and press Insert to
toggle insert/overwrite mode. The classic cross-platform clipboard bindings are
Ctrl+Insert to copy, Shift+Delete to cut, and Shift+Insert to paste; Ctrl+A
selects all. Incoming terminal paste is delivered as one payload and control
characters are normalized before insertion.

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
bounded initial result, or Enter to open its searchable relationship window.
Up/Down moves through matches, F2 opens the selected related record, Enter
applies it, and Esc returns one window at a time. Optional application metadata
configures display/search fields, the result bound, and drill-down policy;
Vulpes always stores the underlying key. When an existing relationship falls
outside the initial result page, the form resolves that exact row independently
so it never substitutes a raw key for its display label.

Open the interactive SQL console with `--sql` or `--command sql`. It accepts
cursor-aware multiline input: arrows move by character or line, Home/End move
within a line, PageUp/PageDown move by an editor page, Enter splits a line, Tab
indents to a four-column stop, and Backspace/Delete join lines at their edges.
Selection, Unicode word movement, clipboard commands, insert/overwrite mode,
Ctrl+Z undo, Ctrl+Y redo, and Ctrl+Shift+Z redo use the same editor semantics as
single-line fields. Ctrl+Up/Down walks a bounded 100-entry command history while
preserving the current draft.
The editor follows the caret vertically and horizontally; border arrows mark
hidden content. `F8` executes and presents the final row-producing statement
through the same Grid widget as `browse`. After results appear, `F7` switches
keyboard focus between the editor and result Grid; `Esc` first returns focus to
the editor and then returns to the shell. Execution is bounded to 1,000 result
rows by default, and the console reports truncation and affected-row counts.
Parameter prompts and multiple displayed result sets remain deliberately deferred.

`--command` accepts the database commands plus `forms`, `form <name>`, `views`,
`view <name>`, `reports`, `report <name>`, `export <report> <format> <path>
[overwrite]`, and `run <command-name>`. It is a
non-interactive bridge to the same application command dispatcher used by the
in-app command palette and metadata menus. Named report execution opens a
read-only, bounded result through the shared Grid. `--query` and `--output`
export one read-only SQL statement without opening the workspace. See
[docs/exporting.md](docs/exporting.md) for all formats, safety guarantees, and
examples.

Interface messages use BCP-47 locales and optional UTF-8 JSON catalogs. For
example, use the shipped Czech translation with `--locale cs-CZ --catalog
translations\cs.json`. See [docs/localization.md](docs/localization.md).
ICU MessageFormat supplies translated plural/select grammar, and ICU/CLDR
supplies locale-aware numeric display. Currency and date/time presentation stay
explicit metadata policies rather than being guessed from SQLite declarations.

The inventory and workshop acceptance tests exercise only generic Vulpes
capabilities: schema-driven datasets, transactional editing, searchable
relationships and drill-down, SQL results rendered through Grid, filtering,
searching, and stock movement insertion. They contain no runtime special cases.

See [docs/architecture.md](docs/architecture.md), [docs/development.md](docs/development.md),
[docs/exporting.md](docs/exporting.md), and [TODO.md](TODO.md) before changing
subsystem boundaries or choosing work.

## Project status

Version 0.1 is pre-alpha. APIs are expected to evolve; the data-modifying UI is
currently limited to basic generated forms. Database backups remain the user's
responsibility.

## License

MIT. See [LICENSE](LICENSE).
