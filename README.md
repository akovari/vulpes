# Vulpes

**A keyboard-first, local SQLite application environment.**

Vulpes turns an ordinary SQLite schema into a usable database application: a
developer can browse tables, edit records, follow relationships, run SQL, and
then progressively add application metadata and small Lua business rules. The
first frontend is a terminal UI, but the database and application model are not
coupled to the terminal.

```text
SQLite database -> schema-aware datasets -> application model -> TUI
```

Vulpes is local-first. A small application can be just the Vulpes executable
and one SQLite file. There is no server, browser, Docker installation, or
separate database service.

> **Pre-alpha:** Vulpes is under active development. Back up real databases
> before editing them, and expect public APIs and the UI to evolve.

## What you can do today

- Open a SQLite database or start the workspace without one.
- Browse tables and views with keyboard navigation, sorting, filtering,
  searching, paging, and adjustable columns.
- Insert, edit, and delete records through generated, schema-aware forms.
- Follow foreign keys through searchable relationship lookups and drill-downs.
- Run SQL in a multiline console and render results with the same grid.
- Define optional SQLite-resident forms, views, menus, commands, and reports.
- Export read-only query and report results to CSV, JSON, text, HTML, or PDF.
- Localize the interface with external UTF-8 catalogs; Czech is included.
- Add bounded Lua validation and normalization hooks to trusted application
  databases.

Vulpes is not a server, ORM, web stack, visual form designer, or a replacement
for SQLite tooling. Its immediate goal is a fast, pleasant
**browse -> edit -> save** workflow for local database applications.

## Quick start on Windows

### Requirements

- Visual Studio 2026 with MSVC 18 and C++ desktop tools
- CMake 3.31 or newer
- Ninja 1.12 or newer
- Git

The checked-in vcpkg manifest resolves the C++ dependencies. On a standard
Visual Studio installation, `scripts/dev.ps1` finds the bundled vcpkg and LLVM
tools automatically. Otherwise, set `VCPKG_ROOT` to a vcpkg installation before
building.

```powershell
.\scripts\dev.ps1
& .\build\windows-msvc\Debug\vulpes.exe --version
& .\build\windows-msvc\Debug\vulpes.exe
```

The first command configures, builds, checks formatting, and runs the Debug
test suite. Later invocations reuse the build directory.

### Open a database

```powershell
& .\build\windows-msvc\Debug\vulpes.exe company.db
& .\build\windows-msvc\Debug\vulpes.exe company.db --table customers
& .\build\windows-msvc\Debug\vulpes.exe company.db --command "browse customers"
```

No database path opens the workspace home screen. Use it to open an existing
database, open one read-only, create a database, or reopen a recent entry.
Providing a path opens an existing database read/write **without** create
permission, so a typo does not silently create a new SQLite file.

### Try an example

With the optional `sqlite3` command-line tool installed, create the workshop
example and launch it:

```powershell
sqlite3 workshop.db ".read examples/workshop/schema.sql"
sqlite3 workshop.db ".read examples/workshop/seed.sql"
& .\build\windows-msvc\Debug\vulpes.exe workshop.db
```

The larger inventory example is documented in
[examples/README.md](examples/README.md).

## Using the workspace

Vulpes is designed for the keyboard. These are the most useful starting keys:

| Where | Key | Action |
| --- | --- | --- |
| Workspace | `Ctrl+O` / `Ctrl+N` | Open an existing database / create one |
| Workspace | `Ctrl+P` | Open the command palette |
| Workspace | `F10` or `Alt+F` | Open the File menu |
| Workspace | `Ctrl+Tab` / `Ctrl+W` | Switch tabs / close the active tab |
| Browse | Arrow keys | Move the selected row or column |
| Browse | `F2` / `Insert` / `Delete` | Edit / add / delete a record |
| Browse | `F3` / `F4` / `F5` / `F6` | Search / filter / refresh / sort |
| Form | `F8` / `Esc` | Save / cancel |
| SQL console | `F8` / `F7` | Execute / switch editor-result focus |
| Anywhere | `Esc` / `Ctrl+C` | Close the active overlay / exit cleanly |

Menus support underlined Alt mnemonics, and `Enter` opens relationship lookups
from a generated record form. The File and Database menus also provide an
optional directory browser; direct path entry remains available for network and
known locations.

Use `--help` for the complete command-line reference:

```powershell
& .\build\windows-msvc\Debug\vulpes.exe --help
```

Common non-interactive commands include:

```powershell
# Inspect a database without starting the workspace.
& .\build\windows-msvc\Debug\vulpes.exe company.db --command tables
& .\build\windows-msvc\Debug\vulpes.exe company.db --command "schema customers"

# Export exactly one read-only SQL statement.
& .\build\windows-msvc\Debug\vulpes.exe company.db `
  --query "SELECT name, email FROM customers ORDER BY name" `
  --output customers.csv

# Inspect a terminal host before testing the interactive UI.
& .\build\windows-msvc\Debug\vulpes.exe --terminal-capabilities
& .\build\windows-msvc\Debug\vulpes.exe --terminal-diagnostics
```

## Application databases

Every ordinary SQLite database is a valid Vulpes input. Vulpes inspects its
schema but does not add application tables while browsing it. To enhance a
database with Vulpes application metadata, run the explicit migration first:

```powershell
& .\build\windows-msvc\Debug\vulpes.exe inventory.db --migrate-app
```

That creates or upgrades only Vulpes' reserved `_app_*` tables in one SQLite
transaction; it does not alter business-table definitions. A `.vulpes`
extension is a convention for an application database, not a custom format, so
ordinary SQLite tools remain compatible.

The optional Lua hooks are trusted application code. Their host API has no raw
SQLite, file-system, network, shell, terminal, or UI access, but the restricted
runtime is **not** a security boundary for an untrusted database. Review an
application's scripts before opening it. See [scripting.md](docs/scripting.md).

## Development

On Windows, use the Debug-first development wrapper:

```powershell
.\scripts\dev.ps1 build                  # compile only
.\scripts\dev.ps1 test -CTestRegex dataset # focused tests
.\scripts\dev.ps1 format                 # apply clang-format
.\scripts\dev.ps1 tidy                   # separate clang-tidy build
```

Do not use a Release build for normal iteration. `.\scripts\dev.ps1 release`
and `package` are explicit milestone/release actions.

Linux and macOS use the same CMake configuration through their native presets:

```sh
cmake --preset linux-gcc      # or: macos-clang
cmake --build --preset linux-debug --parallel
ctest --preset linux-debug
```

Both platforms need CMake, Ninja, a current GCC/Clang, and `VCPKG_ROOT` set to
vcpkg. See [development.md](docs/development.md) for the complete workflow,
formatting, static analysis, and pre-commit setup.

## Documentation

| Topic | Start here |
| --- | --- |
| Product boundary and roadmap | [product.md](docs/product.md) and [TODO.md](TODO.md) |
| Architecture and ADRs | [architecture.md](docs/architecture.md) and [docs/adr](docs/adr) |
| Example workflows | [examples/README.md](examples/README.md) |
| Metadata-defined applications | [application-metadata.md](docs/application-metadata.md) |
| Lua business rules | [scripting.md](docs/scripting.md) |
| Localization and formatting | [localization.md](docs/localization.md) |
| Reports and exports | [exporting.md](docs/exporting.md) |
| Backups, recovery, and upgrades | [operations.md](docs/operations.md) |
| Archive, signing, and distribution policy | [releasing.md](docs/releasing.md) and [distribution.md](docs/distribution.md) |

## Release status

Vulpes 0.1 is not yet publishable. Debug development and deterministic tests
run locally; physical terminal verification and release-artifact validation are
still required on Windows, Linux, and macOS. Current release limitations are
tracked in [release-notes.md](docs/release-notes.md) and [TODO.md](TODO.md).

CPack can produce a ZIP archive and SHA-256 sidecar from a clean version tag,
but it is an unsigned pre-release artifact until the documented signing and
notarization policy is implemented and independently verified.

## License

Vulpes is released under the [MIT License](LICENSE). Third-party components and
their licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
